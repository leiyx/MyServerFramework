/**
 * @file tcp_server.h
 * @brief TCP服务器的封装
 */
#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include <functional>
#include <memory>

#include "net/address.h"
#include "config/config.h"
#include "net/iomanager.h"

#include "net/socket.h"

namespace serverframework {
/**
 * @brief TCP服务器封装
 */
class TcpServer : public std::enable_shared_from_this<TcpServer> {
 public:
  using ptr = std::shared_ptr<TcpServer>;
  /**
   * @brief 构造函数
   * @param[in] name 服务器名称
   * @param[in] type 服务器类型
   * @param[in] io_worker socket客户端工作的协程调度器
   * @param[in] accept_worker 服务器socket执行接收socket连接的协程调度器
   */
  TcpServer(serverframework::IOManager* io_woker =
                serverframework::IOManager::GetThis(),
            serverframework::IOManager* accept_worker =
                serverframework::IOManager::GetThis());
  TcpServer(const TcpServer&)=delete;
  TcpServer& operator=(const TcpServer&) = delete;

  /**
   * @brief 析构函数
   */
  virtual ~TcpServer();

  /**
   * @brief 绑定地址
   * @return 返回是否绑定成功
   */
  virtual bool bind(serverframework::Address::ptr addr);

  /**
   * @brief 绑定地址数组
   * @param[in] addrs 需要绑定的地址数组
   * @param[out] fails 绑定失败的地址
   * @return 是否绑定成功
   */
  virtual bool bind(const std::vector<Address::ptr>& addrs,
                    std::vector<Address::ptr>& fails);

  /**
   * @brief 启动服务
   * @pre 需要bind成功后执行
   */
  virtual bool Start();

  /**
   * @brief 停止服务
   */
  virtual void Stop();

  /**
   * @brief 返回读取超时时间(毫秒)
   */
  uint64_t GetRecvTimeout() const { return recv_timeout_; }

  /**
   * @brief 返回服务器名称
   */
  std::string GetName() const { return name_; }

  /**
   * @brief 设置读取超时时间(毫秒)
   */
  void SetRecvTimeout(uint64_t v) { recv_timeout_ = v; }

  /**
   * @brief 设置服务器名称
   */
  virtual void SetName(const std::string& v) { name_ = v; }

  /**
   * @brief 是否停止
   */
  bool IsStop() const { return is_stop_; }

  /**
   * @brief 以字符串形式dump server信息
   */
  virtual std::string ToString(const std::string& prefix = "");

 protected:
  /**
   * @brief 处理新连接的Socket类
   */
  virtual void HandleClient(Socket::ptr client);

  /**
   * @brief 开始接受连接
   */
  virtual void StartAccept(Socket::ptr sock);

 protected:
  // 监听Socket数组
  std::vector<Socket::ptr> socks_;
  // 新连接的Socket工作的调度器
  IOManager* io_worker_;
  // 服务器Socket接收连接的调度器
  IOManager* accept_worker_;
  // 接收超时时间(毫秒)
  uint64_t recv_timeout_;
  // 服务器名称
  std::string name_;
  // 服务器类型
  std::string type_;
  // 服务是否停止
  bool is_stop_;
};

}  // namespace serverframework

#endif
