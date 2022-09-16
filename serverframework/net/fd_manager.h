
/**
 * @file fd_manager.h
 * @brief 文件句柄管理类
 */
#ifndef FD_MANAGER_H
#define FD_MANAGER_H

#include <memory>
#include <vector>

#include "util/singleton.h"
#include "env/thread.h"

namespace serverframework {

/**
 * @brief 文件句柄上下文类
 * @details 管理文件句柄类型(是否socket)
 *          是否阻塞,是否关闭,读/写超时时间
 */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  using ptr = std::shared_ptr<FdCtx>;
  /**
   * @brief 通过文件句柄构造FdCtx
   */
  FdCtx(int fd);
  /**
   * @brief 析构函数
   */
  ~FdCtx();

  /**
   * @brief 是否初始化完成
   */
  bool IsInit() const { return is_init_; }

  /**
   * @brief 是否socket
   */
  bool IsSocket() const { return is_socket_; }

  /**
   * @brief 是否已关闭
   */
  bool IsClose() const { return is_closed_; }

  /**
   * @brief 设置用户主动设置非阻塞
   * @param[in] v 是否阻塞
   */
  void SetUserNonblock(bool v) { user_nonblock_ = v; }

  /**
   * @brief 获取是否用户主动设置的非阻塞
   */
  bool GetUserNonblock() const { return user_nonblock_; }

  /**
   * @brief 设置系统非阻塞
   * @param[in] v 是否阻塞
   */
  void SetSysNonblock(bool v) { sys_nonblock_ = v; }

  /**
   * @brief 获取系统非阻塞
   */
  bool GetSysNonblock() const { return sys_nonblock_; }

  /**
   * @brief 设置超时时间
   * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
   * @param[in] v 时间毫秒
   */
  void SetTimeout(int type, uint64_t v);

  /**
   * @brief 获取超时时间
   * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
   * @return 超时时间毫秒
   */
  uint64_t GetTimeout(int type);

 private:
  /**
   * @brief 初始化
   */
  bool Init();

 private:
  // 是否初始化
  bool is_init_ : 1;
  // 是否socket
  bool is_socket_ : 1;
  // 是否hook非阻塞
  bool sys_nonblock_ : 1;
  // 是否用户主动设置非阻塞
  bool user_nonblock_ : 1;
  // 是否关闭
  bool is_closed_ : 1;
  // 文件句柄
  int fd_;
  // 读超时时间毫秒
  uint64_t recv_timeout_;
  // 写超时时间毫秒
  uint64_t send_timeout_;
};

/**
 * @brief 文件句柄管理类
 */
class FdManager {
 public:
  using RWMutexType = RWMutex;
  /**
   * @brief 无参构造函数
   */
  FdManager();

  /**
   * @brief 获取/创建文件句柄类FdCtx
   * @param[in] fd 文件句柄
   * @param[in] auto_create 是否自动创建
   * @return 返回对应文件句柄类FdCtx::ptr
   */
  FdCtx::ptr Get(int fd, bool auto_create = false);

  /**
   * @brief 删除文件句柄类
   * @param[in] fd 文件句柄
   */
  void Del(int fd);

 private:
  // 读写锁
  RWMutexType mutex_;
  // 文件句柄集合
  std::vector<FdCtx::ptr> datas_;
};

// 文件句柄单例
using FdMgr = Singleton<FdManager>;

}  // namespace serverframework

#endif
