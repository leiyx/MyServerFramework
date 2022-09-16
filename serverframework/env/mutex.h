/**
 * @file mutex.h
 * @brief 信号量，互斥锁，读写锁，范围锁模板，自旋锁，原子锁
 */
#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>



namespace serverframework {

/**
 * @brief 信号量
 */
class Semaphore {
 public:
  /**
   * @brief 构造函数
   * @param[in] count 信号量值的大小
   */
  Semaphore(uint32_t count = 0);
  Semaphore(const Semaphore&)=delete;
  Semaphore& operator=(const Semaphore&) = delete;

  /**
   * @brief 析构函数
   */
  ~Semaphore();

  /**
   * @brief 获取信号量
   */
  void wait();

  /**
   * @brief 释放信号量
   */
  void notify();

 private:
  sem_t semaphore_;
};

/**
 * @brief 局部锁的模板实现
 */
template <class T>
struct ScopedLockImpl {
 public:
  /**
   * @brief 构造函数
   * @param[in] mutex Mutex
   */
  ScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.lock();
    locked_ = true;
  }

  /**
   * @brief 析构函数,自动释放锁
   */
  ~ScopedLockImpl() { unlock(); }

  /**
   * @brief 加锁
   */
  void lock() {
    if (!locked_) {
      mutex_.lock();
      locked_ = true;
    }
  }

  /**
   * @brief 解锁
   */
  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  // mutex
  T &mutex_;
  // 是否已上锁
  bool locked_;
};

/**
 * @brief 局部读锁模板实现
 */
template <class T>
struct ReadScopedLockImpl {
 public:
  /**
   * @brief 构造函数
   * @param[in] mutex 读写锁
   */
  ReadScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.rdlock();
    locked_ = true;
  }

  /**
   * @brief 析构函数,自动释放锁
   */
  ~ReadScopedLockImpl() { unlock(); }

  /**
   * @brief 上读锁
   */
  void lock() {
    if (!locked_) {
      mutex_.rdlock();
      locked_ = true;
    }
  }

  /**
   * @brief 释放锁
   */
  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  // mutex
  T &mutex_;
  // 是否已上锁
  bool locked_;
};

/**
 * @brief 局部写锁模板实现
 */
template <class T>
struct WriteScopedLockImpl {
 public:
  /**
   * @brief 构造函数
   * @param[in] mutex 读写锁
   */
  WriteScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.wrlock();
    locked_ = true;
  }

  /**
   * @brief 析构函数
   */
  ~WriteScopedLockImpl() { unlock(); }

  /**
   * @brief 上写锁
   */
  void lock() {
    if (!locked_) {
      mutex_.wrlock();
      locked_ = true;
    }
  }

  /**
   * @brief 解锁
   */
  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  // Mutex
  T &mutex_;
  // 是否已上锁
  bool locked_;
};

/**
 * @brief 互斥量
 */
class Mutex   {
 public:
  // 局部锁
  typedef ScopedLockImpl<Mutex> Lock;

  /**
   * @brief 构造函数
   */
  Mutex() { pthread_mutex_init(&mutex_, nullptr); }
  Mutex(const Mutex&)=delete;
  Mutex& operator=(const Mutex&) = delete;

  /**
   * @brief 析构函数
   */
  ~Mutex() { pthread_mutex_destroy(&mutex_); }

  /**
   * @brief 加锁
   */
  void lock() { pthread_mutex_lock(&mutex_); }

  /**
   * @brief 解锁
   */
  void unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  // mutex
  pthread_mutex_t mutex_;
};

/**
 * @brief 空锁(用于调试)
 */
class NullMutex  {
 public:
  // 局部锁
  typedef ScopedLockImpl<NullMutex> Lock;

  /**
   * @brief 构造函数
   */
  NullMutex() {}
  NullMutex(const NullMutex&)=delete;
  NullMutex& operator=(const NullMutex&) = delete;

  /**
   * @brief 析构函数
   */
  ~NullMutex() {}

  /**
   * @brief 加锁
   */
  void lock() {}

  /**
   * @brief 解锁
   */
  void unlock() {}
};

/**
 * @brief 读写互斥量
 */
class RWMutex  {
 public:
  // 局部读锁
  typedef ReadScopedLockImpl<RWMutex> ReadLock;

  // 局部写锁
  typedef WriteScopedLockImpl<RWMutex> WriteLock;

  /**
   * @brief 构造函数
   */
  RWMutex() { pthread_rwlock_init(&m_lock, nullptr); }
  RWMutex(const RWMutex&)=delete;
  RWMutex& operator=(const RWMutex&) = delete;

  /**
   * @brief 析构函数
   */
  ~RWMutex() { pthread_rwlock_destroy(&m_lock); }

  /**
   * @brief 上读锁
   */
  void rdlock() { pthread_rwlock_rdlock(&m_lock); }

  /**
   * @brief 上写锁
   */
  void wrlock() { pthread_rwlock_wrlock(&m_lock); }

  /**
   * @brief 解锁
   */
  void unlock() { pthread_rwlock_unlock(&m_lock); }

 private:
  // 读写锁
  pthread_rwlock_t m_lock;
};

/**
 * @brief 空读写锁(用于调试)
 */
class NullRWMutex {
 public:
  // 局部读锁
  typedef ReadScopedLockImpl<NullMutex> ReadLock;
  // 局部写锁
  typedef WriteScopedLockImpl<NullMutex> WriteLock;

  /**
   * @brief 构造函数
   */
  NullRWMutex() {}
  NullRWMutex(const NullRWMutex&)=delete;
  NullRWMutex& operator=(const NullRWMutex&) = delete;

  /**
   * @brief 析构函数
   */
  ~NullRWMutex() {}

  /**
   * @brief 上读锁
   */
  void rdlock() {}

  /**
   * @brief 上写锁
   */
  void wrlock() {}
  /**
   * @brief 解锁
   */
  void unlock() {}
};

/**
 * @brief 自旋锁
 */
class Spinlock {
 public:
  // 局部锁
  typedef ScopedLockImpl<Spinlock> Lock;

  /**
   * @brief 构造函数
   */
  Spinlock() { pthread_spin_init(&mutex_, 0); }
  Spinlock(const Spinlock&)=delete;
  Spinlock& operator=(const Spinlock&) = delete;

  /**
   * @brief 析构函数
   */
  ~Spinlock() { pthread_spin_destroy(&mutex_); }

  /**
   * @brief 上锁
   */
  void lock() { pthread_spin_lock(&mutex_); }

  /**
   * @brief 解锁
   */
  void unlock() { pthread_spin_unlock(&mutex_); }

 private:
  // 自旋锁
  pthread_spinlock_t mutex_;
};

/**
 * @brief 原子锁
 */
class CASLock   {
 public:
  // 局部锁
  typedef ScopedLockImpl<CASLock> Lock;

  /**
   * @brief 构造函数
   */
  CASLock() { mutex_.clear(); }
  CASLock(const CASLock&)=delete;
  CASLock& operator=(const CASLock&) = delete;

  /**
   * @brief 析构函数
   */
  ~CASLock() {}

  /**
   * @brief 上锁
   */
  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&mutex_,
                                                  std::memory_order_acquire))
      ;
  }

  /**
   * @brief 解锁
   */
  void unlock() {
    std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);
  }

 private:
  // 原子状态
  volatile std::atomic_flag mutex_;
};

}  // namespace serverframework

#endif  // MUTEX_H
