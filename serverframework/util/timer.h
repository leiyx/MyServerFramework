/**
 * @file timer.h
 * @brief 定时器封装
 */
#ifndef TIMER_H
#define TIMER_H

#include <memory>
#include <set>
#include <vector>

#include "env/mutex.h"

namespace serverframework {

class TimerManager;
/**
 * @brief 定时器
 */
class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;

 public:
  // 定时器的智能指针类型
  using ptr = std::shared_ptr<Timer>;

  /**
   * @brief 取消定时器
   */
  bool Cancel();

  /**
   * @brief 刷新设置定时器的执行时间
   */
  bool Refresh();

  /**
   * @brief 重置定时器时间
   * @param[in] ms 定时器执行间隔时间(毫秒)
   * @param[in] from_now 是否从当前时间开始计算
   */
  bool reset(uint64_t ms, bool from_now);

 private:
  /**
   * @brief 构造函数
   * @param[in] ms 定时器执行间隔时间
   * @param[in] cb 回调函数
   * @param[in] recurring 是否循环
   * @param[in] manager 定时器管理器
   */
  Timer(uint64_t ms, std::function<void()> cb, bool recurring,
        TimerManager* manager);
  /**
   * @brief 构造函数
   * @param[in] next 执行的时间戳(毫秒)
   */
  Timer(uint64_t next);

 private:
  // 是否循环定时器
  bool recurring_ = false;
  // 执行周期
  uint64_t ms_ = 0;
  // 精确的执行时间
  uint64_t next_ = 0;
  // 回调函数
  std::function<void()> cb_;
  // 定时器管理器
  TimerManager* manager_ = nullptr;

 private:
  /**
   * @brief 定时器比较仿函数
   */
  struct Comparator {
    /**
     * @brief 比较定时器的智能指针的大小(按执行时间排序)
     * @param[in] lhs 定时器智能指针
     * @param[in] rhs 定时器智能指针
     */
    bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
  };
};

/**
 * @brief 定时器管理器
 */
class TimerManager {
  friend class Timer;

 public:
  // 读写锁类型
  typedef RWMutex RWMutexType;

  /**
   * @brief 构造函数
   */
  TimerManager();

  /**
   * @brief 析构函数
   */
  virtual ~TimerManager();

  /**
   * @brief 添加定时器
   * @param[in] ms 定时器执行间隔时间
   * @param[in] cb 定时器回调函数
   * @param[in] recurring 是否循环定时器
   */
  Timer::ptr AddTimer(uint64_t ms, std::function<void()> cb,
                      bool recurring = false);

  /**
   * @brief 添加条件定时器
   * @param[in] ms 定时器执行间隔时间
   * @param[in] cb 定时器回调函数
   * @param[in] weak_cond 条件
   * @param[in] recurring 是否循环
   */
  Timer::ptr AddConditionTimer(uint64_t ms, std::function<void()> cb,
                               std::weak_ptr<void> weak_cond,
                               bool recurring = false);

  /**
   * @brief 到最近一个定时器执行的时间间隔(毫秒)
   */
  uint64_t GetNextTimer();

  /**
   * @brief 获取需要执行的定时器的回调函数列表
   * @param[out] cbs 回调函数数组
   */
  void ListExpiredCb(std::vector<std::function<void()> >& cbs);

  /**
   * @brief 是否有定时器
   */
  bool HasTimer();

 protected:
  /**
   * @brief 当有新的定时器插入到定时器的首部,执行该函数
   */
  virtual void OnTimerInsertedAtFront() = 0;

  /**
   * @brief 将定时器添加到管理器中
   */
  void AddTimer(Timer::ptr val, RWMutexType::WriteLock& lock);

 private:
  /**
   * @brief 检测服务器时间是否被调后了
   */
  bool DetectClockRollover(uint64_t now_ms);

 private:
  // Mutex
  RWMutexType mutex_;
  // 定时器集合
  std::set<Timer::ptr, Timer::Comparator> timers_;
  // 是否触发onTimerInsertedAtFront
  bool tickled_ = false;
  // 上次执行时间
  uint64_t previous_use_time_ = 0;
};

}  // namespace serverframework

#endif
