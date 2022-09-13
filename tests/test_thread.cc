/**
 * @file test_thread.cc
 * @brief 线程模块测试
 */
#include "serverframework/serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

int count = 0;
serverframework::Mutex s_mutex;

void func1(void *arg) {
  LOG_INFO(g_logger) << "name:" << serverframework::Thread::GetNameS()
                     << " this.name:"
                     << serverframework::Thread::GetThis()->GetName()
                     << " thread name:" << serverframework::GetThreadName()
                     << " id:" << serverframework::GetThreadId() << " this.id:"
                     << serverframework::Thread::GetThis()->GetId();
  LOG_INFO(g_logger) << "arg: " << *(int *)arg;
  for (int i = 0; i < 10000; i++) {
    serverframework::Mutex::Lock lock(s_mutex);
    ++count;
  }
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  std::vector<serverframework::Thread::ptr> thrs;
  int arg = 123456;
  for (int i = 0; i < 3; i++) {
    // 带参数的线程用std::bind进行参数绑定
    serverframework::Thread::ptr thr(new serverframework::Thread(
        std::bind(func1, &arg), "thread_" + std::to_string(i)));
    thrs.push_back(thr);
  }

  for (int i = 0; i < 3; i++) {
    thrs[i]->Join();
  }

  LOG_INFO(g_logger) << "count = " << count;
  return 0;
}
