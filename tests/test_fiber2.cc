/**
 * @file test_fiber2.cc
 * @brief 协程测试，用于演示非对称协程切换及跑飞的情况
 */
#include <string>
#include <vector>

#include "fiber/fiber.h"
#include "serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

void run_in_fiber2() {
  LOG_INFO(g_logger) << "run_in_fiber2 begin";
  LOG_INFO(g_logger) << "run_in_fiber2 end";
}

void run_in_fiber() {
  LOG_INFO(g_logger) << "run_in_fiber begin";

  /**
   * 非对称协程，子协程不能创建并运行新的子协程，下面的操作是有问题的，
   * 子协程再创建子协程，原来的主协程就跑飞了
   * 执行报错：*** stack smashing detected ***: terminated
   */
  serverframework::Fiber::ptr fiber(
      new serverframework::Fiber(run_in_fiber2, 0, false));
  fiber->Resume();

  LOG_INFO(g_logger) << "run_in_fiber end";
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  LOG_INFO(g_logger) << "main begin";

  serverframework::Fiber::GetThis();

  serverframework::Fiber::ptr fiber(
      new serverframework::Fiber(run_in_fiber, 0, false));
  fiber->Resume();

  LOG_INFO(g_logger) << "main end";
  return 0;
}