/**
 * @file test_fiber.cc
 * @brief 协程测试基本接口测试
 */
#include <string>
#include <vector>

#include "serverframework/serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

void run_in_fiber2() {
  LOG_INFO(g_logger) << "run_in_fiber2 begin";
  LOG_INFO(g_logger) << "run_in_fiber2 end";
}

void run_in_fiber() {
  LOG_INFO(g_logger) << "run_in_fiber begin";

  LOG_INFO(g_logger) << "before run_in_fiber Yield";
  serverframework::Fiber::GetThis()->Yield();
  LOG_INFO(g_logger) << "after run_in_fiber Yield";

  LOG_INFO(g_logger) << "run_in_fiber end";
  // fiber结束之后会自动返回主协程运行
}

void test_fiber() {
  LOG_INFO(g_logger) << "test_fiber begin";

  // 初始化线程主协程
  serverframework::Fiber::GetThis();

  serverframework::Fiber::ptr fiber(
      new serverframework::Fiber(run_in_fiber, 0, false));
  LOG_INFO(g_logger) << "use_count:" << fiber.use_count();  // 1

  LOG_INFO(g_logger) << "before test_fiber Resume";
  fiber->Resume();
  LOG_INFO(g_logger) << "after test_fiber Resume";

  /**
   * 关于fiber智能指针的引用计数为3的说明：
   * 一份在当前函数的fiber指针，一份在MainFunc的cur指针
   * 还有一份在在run_in_fiber的GetThis()结果的临时变量里
   */
  LOG_INFO(g_logger) << "use_count:" << fiber.use_count();  // 3

  LOG_INFO(g_logger) << "fiber status: " << fiber->GetState();  // READY

  LOG_INFO(g_logger) << "before test_fiber Resume again";
  fiber->Resume();
  LOG_INFO(g_logger) << "after test_fiber Resume again";

  LOG_INFO(g_logger) << "use_count:" << fiber.use_count();      // 1
  LOG_INFO(g_logger) << "fiber status: " << fiber->GetState();  // TERM

  fiber->Reset(
      run_in_fiber2);  // 上一个协程结束之后，复用其栈空间再创建一个新协程
  fiber->Resume();

  LOG_INFO(g_logger) << "use_count:" << fiber.use_count();  // 1
  LOG_INFO(g_logger) << "test_fiber end";
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  serverframework::SetThreadName("main_thread");
  LOG_INFO(g_logger) << "main begin";

  std::vector<serverframework::Thread::ptr> thrs;
  for (int i = 0; i < 2; i++) {
    thrs.push_back(serverframework::Thread::ptr(new serverframework::Thread(
        &test_fiber, "thread_" + std::to_string(i))));
  }

  for (auto i : thrs) {
    i->Join();
  }

  LOG_INFO(g_logger) << "main end";
  return 0;
}