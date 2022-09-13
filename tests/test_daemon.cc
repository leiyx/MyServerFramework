/**
 * @file test_daemon.cc
 * @brief 守护进程测试
 */

#include "serverframework/serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

serverframework::Timer::ptr timer;
int server_main(int argc, char **argv) {
  LOG_INFO(g_logger)
      << serverframework::ProcessInfoMgr::GetInstance()->ToString();
  serverframework::IOManager iom(1);
  timer = iom.AddTimer(
      1000,
      []() {
        LOG_INFO(g_logger) << "onTimer";
        static int count = 0;
        if (++count > 10) {
          exit(1);
        }
      },
      true);
  return 0;
}

int main(int argc, char **argv) {
  return serverframework::StartDaemon(argc, argv, server_main, argc != 1);
}
