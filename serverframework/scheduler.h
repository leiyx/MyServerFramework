/**
 * @file scheduler.h
 * @brief 协程调度器实现
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "fiber.h"
#include "log.h"
#include "thread.h"

namespace serverframework {

/**
 * @brief 协程调度器
 * @details 封装的是N-M的协程调度器, 内部有一个线程池,支持协程在线程池里面切换
 */
class Scheduler {
 public:
  using ptr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  /**
   * @brief 创建调度器
   * @param[in] threads 线程数
   * @param[in] use_caller 是否将当前线程也作为调度线程
   * @param[in] name 名称
   */
  Scheduler(size_t threads = 1, bool use_caller = true,
            const std::string &name = "Scheduler");

  /**
   * @brief 析构函数
   */
  virtual ~Scheduler();

  /**
   * @brief 获取调度器名称
   */
  const std::string &GetName() const { return name_; }

  /**
   * @brief 获取当前线程调度器指针
   */
  static Scheduler *GetThis();

  /**
   * @brief 获取当前线程的调度协程
   */
  static Fiber *GetSchedulerFiber();

  /**
   * @brief 添加调度任务
   * @tparam FiberOrCb 调度任务类型，可以是协程对象或函数指针
   * @param[in] fc 协程对象或指针
   * @param[in] thread 指定运行该任务的线程号，-1表示任意线程
   */
  template <class FiberOrCb>
  void Schedule(FiberOrCb fc, int thread = -1) {
    bool need_tickle = false;
    {
      MutexType::Lock lock(mutex_);
      need_tickle = ScheduleNoLock(fc, thread);
    }

    if (need_tickle) {
      Tickle();  // 唤醒idle协程
    }
  }

  /**
   * @brief 启动调度器
   */
  void Start();

  /**
   * @brief 停止调度器，等所有调度任务都执行完了再返回
   */
  void Stop();

 protected:
  /**
   * @brief 通知协程调度器有任务了
   */
  virtual void Tickle();

  /**
   * @brief 协程调度函数
   */
  void Run();

  /**
   * @brief 无任务调度时执行idle协程
   */
  virtual void Idle();

  /**
   * @brief 返回是否可以停止
   */
  virtual bool Stopping();

  /**
   * @brief 设置当前的协程调度器
   */
  void SetThis();

  /**
   * @brief 返回是否有空闲线程
   * @details 当调度协程进入idle时空闲线程数加1，从idle协程返回时空闲线程数减1
   */
  bool HasIdleThreads() { return idle_thread_count_ > 0; }

 private:
  /**
   * @brief 添加调度任务，无锁
   * @tparam FiberOrCb 调度任务类型，可以是协程对象或函数指针
   * @param[] fc 协程对象或指针
   * @param[] thread 指定运行该任务的线程号，-1表示任意线程
   */
  template <class FiberOrCb>
  bool ScheduleNoLock(FiberOrCb fc, int thread) {
    bool need_tickle = tasks_.empty();
    ScheduleTask task(fc, thread);
    if (task.fiber || task.cb) {
      tasks_.push_back(task);
    }
    return need_tickle;
  }

 private:
  /**
   * @brief 调度任务，协程/函数二选一，可指定在哪个线程上调度
   */
  struct ScheduleTask {
    Fiber::ptr fiber;
    std::function<void()> cb;
    int thread = -1;

    ScheduleTask(Fiber::ptr f, int thr) {
      fiber = f;
      thread = thr;
    }
    ScheduleTask(Fiber::ptr *f, int thr) {
      fiber.swap(*f);
      thread = thr;
    }
    ScheduleTask(std::function<void()> f, int thr) {
      cb = f;
      thread = thr;
    }
    ScheduleTask() { thread = -1; }

    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

 private:
  // 协程调度器名称
  std::string name_;
  // 互斥锁
  MutexType mutex_;
  // 线程池
  std::vector<Thread::ptr> threads_;
  // 任务队列
  std::list<ScheduleTask> tasks_;
  // 线程池的线程ID数组
  std::vector<int> thread_ids_;
  // 调度线程数量，这个值不包含调度器所在的线程
  size_t thread_count_ = 0;
  // 活跃线程数
  std::atomic<size_t> active_thread_count_ = {0};
  // idle线程数
  std::atomic<size_t> idle_thread_count_ = {0};

  // 是否use caller
  bool use_caller_;
  // use_caller为true时，调度器所在线程的调度协程
  Fiber::ptr root_fiber_;
  // use_caller为true时，调度器所在线程的id
  int root_thread_ = 0;

  // 是否正在停止
  bool stopping_ = false;
};

}  // end namespace serverframework

#endif
