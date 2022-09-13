/**
 * @file thread.cc
 * @brief 线程封装实现
 */
#include "thread.h"

#include "log.h"
#include "util.h"

namespace serverframework {

static thread_local Thread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

static serverframework::Logger::ptr g_logger = LOG_NAME("system");

Thread *Thread::GetThis() { return t_thread; }

const std::string &Thread::GetNameS() { return t_thread_name; }

void Thread::SetName(const std::string &name) {
  if (name.empty()) {
    return;
  }
  if (t_thread) {
    t_thread->name_ = name;
  }
  t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name)
    : cb_(cb), name_(name) {
  if (name.empty()) {
    name_ = "UNKNOW";
  }
  int rt = pthread_create(&thread_, nullptr, &Thread::Run, this);
  if (rt) {
    LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
                        << " name=" << name;
    throw std::logic_error("pthread_create error");
  }
  semaphore_.wait();
}

Thread::~Thread() {
  if (thread_) {
    pthread_detach(thread_);
  }
}

void Thread::Join() {
  if (thread_) {
    int rt = pthread_join(thread_, nullptr);
    if (rt) {
      LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                          << " name=" << name_;
      throw std::logic_error("pthread_join error");
    }
    thread_ = 0;
  }
}

void *Thread::Run(void *arg) {
  Thread *thread = (Thread *)arg;
  t_thread = thread;
  t_thread_name = thread->name_;
  thread->id_ = serverframework::GetThreadId();
  pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->cb_);

  thread->semaphore_.notify();

  cb();
  return 0;
}

}  // namespace serverframework
