/**
 * @file thread.h
 * @brief 线程相关的封装
 */

#ifndef THREAD_H
#define THREAD_H

#include <string>

#include "mutex.h"
namespace serverframework {

/**
 * @brief 线程类
 */
class Thread : Noncopyable {
 public:
  // 线程智能指针类型
  using ptr = std::shared_ptr<Thread>;

  /**
   * @brief 构造函数
   * @param[in] cb 线程执行函数
   * @param[in] name 线程名称
   */
  Thread(std::function<void()> cb, const std::string &name);

  /**
   * @brief 析构函数
   */
  ~Thread();

  /**
   * @brief 线程ID
   */
  pid_t GetId() const { return id_; }

  /**
   * @brief 线程名称
   */
  const std::string &GetName() const { return name_; }

  /**
   * @brief 等待线程执行完成
   */
  void Join();

  /**
   * @brief 获取当前的线程指针
   */
  static Thread *GetThis();

  /**
   * @brief 获取当前的线程名称
   */
  static const std::string &GetNameS();

  /**
   * @brief 设置当前线程名称
   * @param[in] name 线程名称
   */
  static void SetName(const std::string &name);

 private:
  /**
   * @brief 线程执行函数
   */
  static void *Run(void *arg);

 private:
  // 线程id
  pid_t id_ = -1;
  // 线程结构
  pthread_t thread_ = 0;
  // 线程执行函数
  std::function<void()> cb_;
  // 线程名称
  std::string name_;
  // 信号量
  Semaphore semaphore_;
};

}  // namespace serverframework

#endif
