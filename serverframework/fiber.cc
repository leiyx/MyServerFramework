/**
 * @file fiber.cpp
 * @brief 协程实现
 */

#include "fiber.h"

#include <atomic>

#include "config.h"
#include "log.h"
#include "macro.h"
#include "scheduler.h"

namespace serverframework {

static Logger::ptr g_logger = LOG_NAME("system");

// 全局静态变量，用于生成协程id
static std::atomic<uint64_t> s_fiber_id{0};
// 全局静态变量，用于统计当前的协程数
static std::atomic<uint64_t> s_fiber_count{0};

// 线程局部变量，当前线程正在运行的协程
static thread_local Fiber *t_fiber = nullptr;
// 线程局部变量，当前线程的主协程，切换到这个协程，就相当于切换到了主线程中运行，智能指针形式
static thread_local Fiber::ptr t_thread_fiber = nullptr;

//协程栈大小，可通过配置文件获取，默认128k
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = Config::Lookup<uint32_t>(
    "fiber.stack_size", 128 * 1024, "fiber stack size");

/**
 * @brief malloc栈内存分配器
 */
class MallocStackAllocator {
 public:
  static void *Alloc(size_t size) { return malloc(size); }
  static void Dealloc(void *vp, size_t size) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->GetId();
  }
  return 0;
}

Fiber::Fiber() {
  SetThis(this);
  state_ = RUNNING;

  if (getcontext(&ctx_)) {
    ASSERT2(false, "getcontext");
  }

  ++s_fiber_count;
  id_ = s_fiber_id++;  // 协程id从0开始，用完加1

  LOG_DEBUG(g_logger) << "Fiber::Fiber() main id = " << id_;
}

void Fiber::SetThis(Fiber *f) { t_fiber = f; }

/**
 * 获取当前协程，同时充当初始化当前线程主协程的作用，这个函数在使用协程之前要调用一下
 */
Fiber::ptr Fiber::GetThis() {
  if (t_fiber) {
    return t_fiber->shared_from_this();
  }

  Fiber::ptr main_fiber(new Fiber);
  ASSERT(t_fiber == main_fiber.get());
  t_thread_fiber = main_fiber;
  return t_fiber->shared_from_this();
}

/**
 * 带参数的构造函数用于创建子协程(任务协程)，需要分配栈
 */
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
    : id_(s_fiber_id++), cb_(cb), run_in_scheduler_(run_in_scheduler) {
  ++s_fiber_count;
  stack_size_ = stacksize ? stacksize : g_fiber_stack_size->GetValue();
  stack_ = StackAllocator::Alloc(stack_size_);

  if (getcontext(&ctx_)) {
    ASSERT2(false, "getcontext");
  }

  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_;
  ctx_.uc_stack.ss_size = stack_size_;

  makecontext(&ctx_, &Fiber::MainFunc, 0);

  LOG_DEBUG(g_logger) << "Fiber::Fiber() id = " << id_;
}

/**
 * 线程的主协程析构时需要特殊处理，因为主协程没有分配栈和cb
 */
Fiber::~Fiber() {
  LOG_DEBUG(g_logger) << "Fiber::~Fiber() id = " << id_;
  --s_fiber_count;
  if (stack_) {
    // 有栈，说明是子协程，需要确保子协程一定是结束状态
    ASSERT(state_ == TERM);
    StackAllocator::Dealloc(stack_, stack_size_);
    LOG_DEBUG(g_logger) << "dealloc stack, id = " << id_;
  } else {
    // 没有栈，说明是线程的主协程
    ASSERT(!cb_);               // 主协程没有cb
    ASSERT(state_ == RUNNING);  // 主协程一定是执行状态

    Fiber *cur = t_fiber;  // 当前协程就是自己
    if (cur == this) {
      SetThis(nullptr);
    }
  }
}

/**
 * 简化状态管理，强制只有TERM状态的协程才可以重置，但其实刚创建好但没执行过的协程(INIT状态)也应该允许重置的
 */
void Fiber::Reset(std::function<void()> cb) {
  ASSERT(stack_);
  ASSERT(state_ == TERM);
  cb_ = cb;
  if (getcontext(&ctx_)) {
    ASSERT2(false, "getcontext");
  }

  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_;
  ctx_.uc_stack.ss_size = stack_size_;

  makecontext(&ctx_, &Fiber::MainFunc, 0);
  state_ = READY;
}

void Fiber::Resume() {
  ASSERT(state_ != TERM && state_ != RUNNING);
  SetThis(this);
  state_ = RUNNING;

  // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
  if (run_in_scheduler_) {
    if (swapcontext(&(Scheduler::GetSchedulerFiber()->ctx_), &ctx_)) {
      ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&(t_thread_fiber->ctx_), &ctx_)) {
      ASSERT2(false, "swapcontext");
    }
  }
}

void Fiber::Yield() {
  ASSERT(state_ == RUNNING || state_ == TERM);
  SetThis(t_thread_fiber.get());
  // 协程运行完之后会自动yield一次，用于回到主协程，此时状态已为结束状态
  if (state_ != TERM) {
    state_ = READY;
  }

  // 如果协程参与调度器调度，那么应该和调度器的主协程进行swap，而不是线程主协程
  if (run_in_scheduler_) {
    if (swapcontext(&ctx_, &(Scheduler::GetSchedulerFiber()->ctx_))) {
      ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&ctx_, &(t_thread_fiber->ctx_))) {
      ASSERT2(false, "swapcontext");
    }
  }
}

/**
 * 这里没有处理协程函数出现异常的情况，同样是为了简化状态管理，并且个人认为协程的异常不应该由框架处理，应该由开发者自行处理
 */
void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();  // GetThis()的shared_from_this()方法让引用计数加1
  ASSERT(cur);

  cur->cb_();
  cur->cb_ = nullptr;
  cur->state_ = TERM;

  auto raw_ptr = cur.get();  // 手动让t_fiber的引用计数减1
  cur.reset();
  raw_ptr->Yield();
}

}  // namespace serverframework