/**
 * @file test_hook.cc
 * @brief hook模块测试
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "serverframework/serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

/**
 * @brief 测试sleep被hook之后的浆果
 */
void test_sleep() {
  LOG_INFO(g_logger) << "test_sleep begin";
  serverframework::IOManager iom;

  /**
   * 这里的两个协程sleep是同时开始的，一共只会睡眠3秒钟，第一个协程开始sleep后，会yield到后台，
   * 第二个协程会得到执行，最终两个协程都会yield到后台，并等待睡眠时间结束，相当于两个sleep是同一起点开始的
   */
  iom.Schedule([] {
    sleep(2);
    LOG_INFO(g_logger) << "sleep 2";
  });

  iom.Schedule([] {
    sleep(3);
    LOG_INFO(g_logger) << "sleep 3";
  });

  LOG_INFO(g_logger) << "test_sleep end";
}

/**
 * 测试socket api hook
 */
void test_sock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, "36.152.44.96", &addr.sin_addr.s_addr);

  LOG_INFO(g_logger) << "begin connect";
  int rt = connect(sock, (const sockaddr *)&addr, sizeof(addr));
  LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

  if (rt) {
    return;
  }

  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  rt = send(sock, data, sizeof(data), 0);
  LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

  if (rt <= 0) {
    return;
  }

  std::string buff;
  buff.resize(4096);

  rt = recv(sock, &buff[0], buff.size(), 0);
  LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

  if (rt <= 0) {
    return;
  }

  buff.resize(rt);
  LOG_INFO(g_logger) << buff;
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  // test_sleep();

  // 只有以协程调度的方式运行hook才能生效
  serverframework::IOManager iom;
  iom.Schedule(test_sock);

  LOG_INFO(g_logger) << "main end";
  return 0;
}