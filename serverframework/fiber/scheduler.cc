/**
 * @file scheduler.cc
 * @brief 协程调度器实现
 */
#include "fiber/scheduler.h"

#include "net/hook.h"
#include "util/macro.h"

namespace serverframework {

static serverframework::Logger::ptr g_logger = LOG_NAME("system");

// 当前线程的调度器，同一个调度器下的所有线程共享同一个实例
static thread_local Scheduler *t_scheduler = nullptr;
// 当前线程的调度协程，每个线程都独有一份
static thread_local Fiber *t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) {
  ASSERT(threads > 0);

  use_caller_ = use_caller;
  name_ = name;

  if (use_caller) {
    --threads;
    serverframework::Fiber::GetThis();
    ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    /**
     * caller线程的主协程不会被线程的调度协程run进行调度，
     * 而且，线程的调度协程停止时，应该返回caller线程的主协程
     * 在user caller情况下，把caller线程的主协程暂时保存起来，
     * 等调度协程结束时，再resume caller协程
     */
    root_fiber_.reset(new Fiber(std::bind(&Scheduler::Run, this), 0, false));

    serverframework::Thread::SetName(name_);
    t_scheduler_fiber = root_fiber_.get();
    root_thread_ = serverframework::GetThreadId();
    thread_ids_.push_back(root_thread_);
  } else {
    root_thread_ = -1;
  }
  thread_count_ = threads;
}

Scheduler *Scheduler::GetThis() { return t_scheduler; }

Fiber *Scheduler::GetSchedulerFiber() { return t_scheduler_fiber; }

void Scheduler::SetThis() { t_scheduler = this; }

Scheduler::~Scheduler() {
  LOG_DEBUG(g_logger) << "Scheduler::~Scheduler()";
  ASSERT(stopping_);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

void Scheduler::Start() {
  LOG_DEBUG(g_logger) << "Start";
  MutexType::Lock lock(mutex_);
  if (stopping_) {
    LOG_ERROR(g_logger) << "Scheduler is stopped";
    return;
  }
  ASSERT(threads_.empty());
  threads_.resize(thread_count_);
  // 开启线程池
  for (size_t i = 0; i < thread_count_; i++) {
    threads_[i].reset(new Thread(std::bind(&Scheduler::Run, this),
                                 name_ + "_" + std::to_string(i)));
    thread_ids_.push_back(threads_[i]->GetId());
  }
}

bool Scheduler::Stopping() {
  MutexType::Lock lock(mutex_);
  return stopping_ && tasks_.empty() && active_thread_count_ == 0;
}

// 这里不做任何事，仅仅是忙等
void Scheduler::Tickle() { LOG_DEBUG(g_logger) << "ticlke"; }

// 这里不做任何事，当调度不能停止时，立即让出CPU
void Scheduler::Idle() {
  LOG_DEBUG(g_logger) << "Idle";
  while (!Stopping()) {
    serverframework::Fiber::GetThis()->Yield();
  }
}

// 优雅停止实现
void Scheduler::Stop() {
  LOG_DEBUG(g_logger) << "Stop";
  if (Stopping()) {
    return;
  }
  stopping_ = true;

  // 如果use caller，那只能由caller线程发起stop
  if (use_caller_) {
    ASSERT(GetThis() == this);
  } else {
    ASSERT(GetThis() != this);
  }

  // 唤醒每个调度线程
  for (size_t i = 0; i < thread_count_; i++) {
    Tickle();
  }
  // 如果调度器所在线程参与调度，则该操作很重要
  if (root_fiber_) {
    Tickle();
  }

  // 在use caller情况下，调度器协程结束时，应该返回caller协程
  // 只使用调度器所在线程进行调度时的协程调度全靠root_fiber_->Resume()去执行所有任务协程
  if (root_fiber_) {
    root_fiber_->Resume();
    LOG_DEBUG(g_logger) << "root_fiber_ end";
  }

  std::vector<Thread::ptr> thrs;
  {
    MutexType::Lock lock(mutex_);
    thrs.swap(threads_);
  }
  // 等待所有调度线程结束
  for (auto &i : thrs) {
    i->Join();
  }
}

void Scheduler::Run() {
  LOG_DEBUG(g_logger) << "Run";
  SetHookEnable(true);
  SetThis();
  if (serverframework::GetThreadId() != root_thread_) {
    t_scheduler_fiber = serverframework::Fiber::GetThis().get();
  }

  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::Idle, this)));
  Fiber::ptr cb_fiber;

  ScheduleTask task;
  while (true) {
    task.reset();
    bool tickle_me = false;  // 是否tickle其他线程进行任务调度
    {
      MutexType::Lock lock(mutex_);
      auto it = tasks_.begin();
      // 遍历所有调度任务
      while (it != tasks_.end()) {
        if (it->thread != -1 && it->thread != serverframework::GetThreadId()) {
          // 指定了调度线程，但不是在当前线程上调度，标记一下需要通知其他线程进行调度，然后跳过这个任务，继续下一个
          ++it;
          tickle_me = true;
          continue;
        }

        // 找到一个未指定线程，或是指定了当前线程的任务
        ASSERT(it->fiber || it->cb);

        // [BUG FIX]: hook
        // IO相关的系统调用时，在检测到IO未就绪的情况下，会先添加对应的读写事件，再yield当前协程，等IO就绪后再resume当前协程
        // 多线程高并发情境下，有可能发生刚添加事件就被触发的情况，如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍为RUNNING的情况
        // 这里简单地跳过这种情况，以损失一点性能为代价，否则整个协程框架都要大改
        if (it->fiber && it->fiber->GetState() == Fiber::RUNNING) {
          ++it;
          continue;
        }

        // 当前调度线程找到一个任务，准备开始调度，将其从任务队列中剔除，活动线程数加1
        task = *it;
        tasks_.erase(it++);
        ++active_thread_count_;
        break;
      }
      // 当前线程拿完一个任务后，发现任务队列还有剩余，那么tickle一下其他线程
      tickle_me |= (it != tasks_.end());
    }

    if (tickle_me) {
      Tickle();
    }

    if (task.fiber) {
      // resume协程，resume返回时，协程要么执行完了，要么半路yield了，总之这个任务就算完成了，活跃线程数减一
      task.fiber->Resume();
      --active_thread_count_;
      task.reset();
    } else if (task.cb) {
      if (cb_fiber) {
        cb_fiber->Reset(task.cb);
      } else {
        cb_fiber.reset(new Fiber(task.cb));
      }
      task.reset();
      cb_fiber->Resume();
      --active_thread_count_;
      cb_fiber.reset();
    } else {
      // 进到这个分支情况一定是任务队列空了，调度idle协程即可
      if (idle_fiber->GetState() == Fiber::TERM) {
        // 如果调度器没有调度任务，那么idle协程会不停地resume/Yield，不会结束，如果idle协程结束了，那一定是调度器停止了
        LOG_DEBUG(g_logger) << "Idle fiber term";
        break;
      }
      ++idle_thread_count_;
      idle_fiber->Resume();
      --idle_thread_count_;
    }
  }
  LOG_DEBUG(g_logger) << "Scheduler::Run() exit";
}

}  // end namespace serverframework