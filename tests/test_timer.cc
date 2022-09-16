/**
 * @file test_timer.cc
 * @brief IO协程测试器定时器测试
 */

#include "serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

static int timeout = 1000;
static serverframework::Timer::ptr s_timer;

void timer_callback() {
  LOG_INFO(g_logger) << "timer callback, timeout = " << timeout;
  timeout += 1000;
  if (timeout < 5000) {
    s_timer->reset(timeout, true);
  } else {
    s_timer->Cancel();
  }
}

void test_timer() {
  serverframework::IOManager iom;

  // 循环定时器
  s_timer = iom.AddTimer(1000, timer_callback, true);

  // 单次定时器
  iom.AddTimer(500, [] { LOG_INFO(g_logger) << "500ms timeout"; });
  iom.AddTimer(5000, [] { LOG_INFO(g_logger) << "5000ms timeout"; });
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  test_timer();

  LOG_INFO(g_logger) << "end";

  return 0;
}