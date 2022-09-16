/**
 * @file test_socket.cc
 * @brief 测试Socket类，tcp服务器
 */
#include "serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

void test_tcp_server() {
  int ret;

  auto addr = serverframework::Address::LookupAnyIPAddress("0.0.0.0:12345");
  ASSERT(addr);

  auto socket = serverframework::Socket::CreateTCPSocket();
  ASSERT(socket);

  ret = socket->bind(addr);
  ASSERT(ret);

  LOG_INFO(g_logger) << "bind success";

  ret = socket->listen();
  ASSERT(ret);

  LOG_INFO(g_logger) << socket->ToString();
  LOG_INFO(g_logger) << "listening...";

  while (1) {
    auto client = socket->accept();
    ASSERT(client);
    LOG_INFO(g_logger) << "new client: " << client->ToString();
    client->send("hello world", strlen("hello world"));
    client->close();
  }
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  serverframework::IOManager iom(2);
  iom.Schedule(&test_tcp_server);

  return 0;
}