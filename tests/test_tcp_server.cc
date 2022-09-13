/**
 * @file test_tcp_server.cc
 * @brief TcpServer类测试
 */
#include "serverframework/serverframework.h"

static serverframework::Logger::ptr g_logger = LOG_ROOT();

/**
 * @brief 自定义TcpServer类，重载handleClient方法
 */
class MyTcpServer : public serverframework::TcpServer {
 protected:
  virtual void HandleClient(serverframework::Socket::ptr client) override;
};

void MyTcpServer::HandleClient(serverframework::Socket::ptr client) {
  LOG_INFO(g_logger) << "new client: " << client->ToString();
  static std::string buf;
  buf.resize(4096);
  client->recv(
      &buf[0],
      buf.length());  // 这里有读超时，由tcp_server.read_timeout配置项进行配置，默认120秒
  LOG_INFO(g_logger) << "recv: " << buf;
  client->close();
}

void Run() {
  serverframework::TcpServer::ptr server(
      new MyTcpServer);  // 内部依赖shared_from_this()，所以必须以智能指针形式创建对象
  auto addr = serverframework::Address::LookupAny("0.0.0.0:12345");
  ASSERT(addr);
  std::vector<serverframework::Address::ptr> addrs;
  addrs.push_back(addr);

  std::vector<serverframework::Address::ptr> fails;
  while (!server->bind(addrs, fails)) {
    sleep(2);
  }

  LOG_INFO(g_logger) << "bind success, " << server->ToString();

  server->Start();
}

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  serverframework::IOManager iom(2);
  iom.Schedule(&Run);

  return 0;
}