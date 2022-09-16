/**
 * @file test_socket_tcp_client.cc
 * @brief 测试Socket类，tcp客户端
 */
#include "serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

void test_tcp_client() {
  int ret;

  auto socket = serverframework::Socket::CreateTCPSocket();
  ASSERT(socket);

  auto addr = serverframework::Address::LookupAnyIPAddress("0.0.0.0:12345");
  ASSERT(addr);

  ret = socket->connect(addr);
  ASSERT(ret);

  LOG_INFO(g_logger) << "connect success, peer address: "
                     << socket->GetRemoteAddress()->ToString();

  std::string buffer;
  buffer.resize(1024);
  socket->recv(&buffer[0], buffer.size());
  LOG_INFO(g_logger) << "recv: " << buffer;
  socket->close();

  return;
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  serverframework::IOManager iom;
  iom.Schedule(&test_tcp_client);

  return 0;
}