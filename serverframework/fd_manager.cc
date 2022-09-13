/**
 * @file fd_manager.cc
 * @brief 文件句柄管理类实现
 * @details 只管理socket
 * fd，记录fd是否为socket，用户是否设置非阻塞，系统是否设置非阻塞，send/recv超时时间
 *          提供FdManager单例和get/del方法，用于创建/获取/删除fd
 */
#include "fd_manager.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hook.h"

namespace serverframework {

FdCtx::FdCtx(int fd)
    : is_init_(false),
      is_socket_(false),
      sys_nonblock_(false),
      user_nonblock_(false),
      is_closed_(false),
      fd_(fd),
      recv_timeout_(-1),
      send_timeout_(-1) {
  Init();
}

FdCtx::~FdCtx() {}

bool FdCtx::Init() {
  if (is_init_) {
    return true;
  }
  recv_timeout_ = -1;
  send_timeout_ = -1;

  struct stat fd_stat;
  if (-1 == fstat(fd_, &fd_stat)) {
    is_init_ = false;
    is_socket_ = false;
  } else {
    is_init_ = true;
    is_socket_ = S_ISSOCK(fd_stat.st_mode);
  }

  if (is_socket_) {
    int flags = fcntl_f(fd_, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      fcntl_f(fd_, F_SETFL, flags | O_NONBLOCK);
    }
    sys_nonblock_ = true;
  } else {
    sys_nonblock_ = false;
  }

  user_nonblock_ = false;
  is_closed_ = false;
  return is_init_;
}

void FdCtx::SetTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    recv_timeout_ = v;
  } else {
    send_timeout_ = v;
  }
}

uint64_t FdCtx::GetTimeout(int type) {
  if (type == SO_RCVTIMEO) {
    return recv_timeout_;
  } else {
    return send_timeout_;
  }
}

FdManager::FdManager() { datas_.resize(64); }

FdCtx::ptr FdManager::Get(int fd, bool auto_create) {
  if (fd == -1) {
    return nullptr;
  }
  RWMutexType::ReadLock lock(mutex_);
  if ((int)datas_.size() <= fd) {
    if (auto_create == false) {
      return nullptr;
    }
  } else {
    if (datas_[fd] || !auto_create) {
      return datas_[fd];
    }
  }
  lock.unlock();

  RWMutexType::WriteLock lock2(mutex_);
  FdCtx::ptr ctx(new FdCtx(fd));
  if (fd >= (int)datas_.size()) {
    datas_.resize(fd * 1.5);
  }
  datas_[fd] = ctx;
  return ctx;
}

void FdManager::Del(int fd) {
  RWMutexType::WriteLock lock(mutex_);
  if ((int)datas_.size() <= fd) {
    return;
  }
  datas_[fd].reset();
}

}  // namespace serverframework
