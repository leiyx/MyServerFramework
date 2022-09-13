/**
 * @file fiber.h
 * @brief 协程模块
 * @details 基于ucontext_t实现，非对称有栈协程
 */

#ifndef FIBER_H
#define FIBER_H

#include <ucontext.h>

#include <functional>
#include <memory>

#include "thread.h"

namespace serverframework {
/**
 * @brief 协程类
 */
class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  using ptr = std::shared_ptr<Fiber>;

  /**
   * @brief 协程状态
   * @details
   * 只定义三态转换关系，也就是协程要么正在运行(RUNNING)，要么准备运行(READY)，要么运行结束(TERM)。
   * 不区分协程的初始状态INIT，初始即READY。不区分协程是异常结束还是正常结束，
   * 只要结束就是TERM状态。也不区别HOLD状态，协程只要未结束也非运行态，那就是READY状态。
   */
  enum State {
    // 就绪态，刚创建或者Yield之后的状态
    READY = 0,
    // 运行态，Resume之后的状态
    RUNNING,
    // 结束态，协程的回调函数执行完之后为TERM状态
    TERM
  };

 private:
  /**
   * @brief 构造函数
   * @attention
   * 无参构造函数只用于创建线程的第一个协程，也就是线程主函数对应的协程(该线程的主协程)，
   * 这个协程只能由GetThis()方法调用，所以定义成私有方法
   */
  Fiber();

 public:
  /**
   * @brief 构造函数，用于创建用户协程
   * @param[in] cb 协程入口函数
   * @param[in] stacksize 栈大小
   * @param[in] run_in_scheduler 本协程是否参与调度器调度，默认为true
   */
  Fiber(std::function<void()> cb, size_t stacksize = 0,
        bool run_in_scheduler = true);

  /**
   * @brief 析构函数
   */
  ~Fiber();

  /**
   * @brief 重置协程状态和入口函数，复用栈空间，不重新创建栈
   * @param[in] cb
   */
  void Reset(std::function<void()> cb);

  /**
   * @brief 将当前协程切到到执行状态
   * @details
   * 当前协程和正在运行的协程进行交换，前者状态变为RUNNING，后者状态变为READY
   */
  void Resume();

  /**
   * @brief 当前协程让出执行权
   * @details
   * 当前协程与上次resume时退到后台的协程进行交换，前者状态变为READY，后者状态变为RUNNING
   */
  void Yield();

  /**
   * @brief 获取协程ID
   */
  uint64_t GetId() const { return id_; }

  /**
   * @brief 获取协程状态
   */
  State GetState() const { return state_; }

 public:
  /**
   * @brief 设置当前正在运行的协程，即设置线程局部变量t_fiber的值
   */
  static void SetThis(Fiber *f);

  /**
   * @brief 返回当前线程正在执行的协程
   * @details 如果当前线程还未创建协程，则创建线程的第一个协程，
   * 且该协程为当前线程的主协程，其他协程都通过这个协程来调度，
   * 其他协程结束时,都要切回到主协程，由主协程重新选择新的协程进行resume。
   * @attention
   * 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化线程主协程
   */
  static Fiber::ptr GetThis();

  /**
   * @brief 获取总协程数
   */
  static uint64_t TotalFibers();

  /**
   * @brief 协程入口函数
   * @details 协程入口函数运行完毕会自动Yeild回主协程
   */
  static void MainFunc();

  /**
   * @brief 获取当前协程id
   */
  static uint64_t GetFiberId();

 private:
  // 协程id
  uint64_t id_ = 0;
  // 协程栈大小
  uint32_t stack_size_ = 0;
  // 协程状态
  State state_ = READY;
  // 协程上下文
  ucontext_t ctx_;
  // 协程栈地址
  void *stack_ = nullptr;
  // 协程入口函数
  std::function<void()> cb_;
  // 本协程是否参与调度器调度
  bool run_in_scheduler_;
};

}  // namespace serverframework
#endif