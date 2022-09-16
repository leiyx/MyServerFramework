#include "net/hook.h"

#include <dlfcn.h>

#include "config/config.h"
#include "net/fd_manager.h"
#include "fiber/fiber.h"
#include "net/iomanager.h"
#include "log/log.h"
#include "util/macro.h"

serverframework::Logger::ptr g_logger = LOG_NAME("system");
namespace serverframework {

static serverframework::ConfigVar<int>::ptr g_tcp_connect_timeout =
    serverframework::Config::Lookup("tcp.connect.timeout", 5000,
                                    "tcp connect timeout");

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
  XX(sleep)          \
  XX(usleep)         \
  XX(nanosleep)      \
  XX(socket)         \
  XX(connect)        \
  XX(accept)         \
  XX(read)           \
  XX(readv)          \
  XX(recv)           \
  XX(recvfrom)       \
  XX(recvmsg)        \
  XX(write)          \
  XX(writev)         \
  XX(send)           \
  XX(sendto)         \
  XX(sendmsg)        \
  XX(close)          \
  XX(fcntl)          \
  XX(ioctl)          \
  XX(getsockopt)     \
  XX(setsockopt)

void hook_init() {
  static bool is_inited = false;
  if (is_inited) {
    return;
  }
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
  _HookIniter() {
    hook_init();
    s_connect_timeout = g_tcp_connect_timeout->GetValue();

    g_tcp_connect_timeout->AddListener(
        [](const int &old_value, const int &new_value) {
          LOG_INFO(g_logger) << "tcp connect timeout changed from " << old_value
                             << " to " << new_value;
          s_connect_timeout = new_value;
        });
  }
};

static _HookIniter s_hook_initer;

bool IsHookEnable() { return t_hook_enable; }

void SetHookEnable(bool flag) { t_hook_enable = flag; }

}  // namespace serverframework

struct timer_info {
  int cancelled = 0;
};

template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name,
                     uint32_t event, int timeout_so, Args &&...args) {
  if (!serverframework::t_hook_enable) {
    return fun(fd, std::forward<Args>(args)...);
  }

  serverframework::FdCtx::ptr ctx =
      serverframework::FdMgr::GetInstance()->Get(fd);
  if (!ctx) {
    return fun(fd, std::forward<Args>(args)...);
  }

  if (ctx->IsClose()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->IsSocket() || ctx->GetUserNonblock()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  uint64_t to = ctx->GetTimeout(timeout_so);
  std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR) {
    n = fun(fd, std::forward<Args>(args)...);
  }
  if (n == -1 && errno == EAGAIN) {
    serverframework::IOManager *iom = serverframework::IOManager::GetThis();
    serverframework::Timer::ptr timer;
    std::weak_ptr<timer_info> winfo(tinfo);

    if (to != (uint64_t)-1) {
      timer = iom->AddConditionTimer(
          to,
          [winfo, fd, iom, event]() {
            auto t = winfo.lock();
            if (!t || t->cancelled) {
              return;
            }
            t->cancelled = ETIMEDOUT;
            iom->CancelEvent(fd, (serverframework::IOManager::Event)(event));
          },
          winfo);
    }

    int rt = iom->AddEvent(fd, (serverframework::IOManager::Event)(event));
    if (UNLIKELY(rt)) {
      LOG_ERROR(g_logger) << hook_fun_name << " AddEvent(" << fd << ", "
                          << event << ")";
      if (timer) {
        timer->Cancel();
      }
      return -1;
    } else {
      serverframework::Fiber::GetThis()->Yield();
      if (timer) {
        timer->Cancel();
      }
      if (tinfo->cancelled) {
        errno = tinfo->cancelled;
        return -1;
      }
      goto retry;
    }
  }

  return n;
}

extern "C" {
#define XX(name) name##_fun name##_f = nullptr;
HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
  if (!serverframework::t_hook_enable) {
    return sleep_f(seconds);
  }

  serverframework::Fiber::ptr fiber = serverframework::Fiber::GetThis();
  serverframework::IOManager *iom = serverframework::IOManager::GetThis();
  iom->AddTimer(seconds * 1000,
                std::bind((void(serverframework::Scheduler::*)(
                              serverframework::Fiber::ptr, int thread)) &
                              serverframework::IOManager::Schedule,
                          iom, fiber, -1));
  serverframework::Fiber::GetThis()->Yield();
  return 0;
}

int usleep(useconds_t usec) {
  if (!serverframework::t_hook_enable) {
    return usleep_f(usec);
  }
  serverframework::Fiber::ptr fiber = serverframework::Fiber::GetThis();
  serverframework::IOManager *iom = serverframework::IOManager::GetThis();
  iom->AddTimer(usec / 1000,
                std::bind((void(serverframework::Scheduler::*)(
                              serverframework::Fiber::ptr, int thread)) &
                              serverframework::IOManager::Schedule,
                          iom, fiber, -1));
  serverframework::Fiber::GetThis()->Yield();
  return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  if (!serverframework::t_hook_enable) {
    return nanosleep_f(req, rem);
  }

  int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
  serverframework::Fiber::ptr fiber = serverframework::Fiber::GetThis();
  serverframework::IOManager *iom = serverframework::IOManager::GetThis();
  iom->AddTimer(timeout_ms,
                std::bind((void(serverframework::Scheduler::*)(
                              serverframework::Fiber::ptr, int thread)) &
                              serverframework::IOManager::Schedule,
                          iom, fiber, -1));
  serverframework::Fiber::GetThis()->Yield();
  return 0;
}

int socket(int domain, int type, int protocol) {
  if (!serverframework::t_hook_enable) {
    return socket_f(domain, type, protocol);
  }
  int fd = socket_f(domain, type, protocol);
  if (fd == -1) {
    return fd;
  }
  serverframework::FdMgr::GetInstance()->Get(fd, true);
  return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen,
                         uint64_t timeout_ms) {
  if (!serverframework::t_hook_enable) {
    return connect_f(fd, addr, addrlen);
  }
  serverframework::FdCtx::ptr ctx =
      serverframework::FdMgr::GetInstance()->Get(fd);
  if (!ctx || ctx->IsClose()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->IsSocket()) {
    return connect_f(fd, addr, addrlen);
  }

  if (ctx->GetUserNonblock()) {
    return connect_f(fd, addr, addrlen);
  }

  int n = connect_f(fd, addr, addrlen);
  if (n == 0) {
    return 0;
  } else if (n != -1 || errno != EINPROGRESS) {
    return n;
  }

  serverframework::IOManager *iom = serverframework::IOManager::GetThis();
  serverframework::Timer::ptr timer;
  std::shared_ptr<timer_info> tinfo(new timer_info);
  std::weak_ptr<timer_info> winfo(tinfo);

  if (timeout_ms != (uint64_t)-1) {
    timer = iom->AddConditionTimer(
        timeout_ms,
        [winfo, fd, iom]() {
          auto t = winfo.lock();
          if (!t || t->cancelled) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->CancelEvent(fd, serverframework::IOManager::WRITE);
        },
        winfo);
  }

  int rt = iom->AddEvent(fd, serverframework::IOManager::WRITE);
  if (rt == 0) {
    serverframework::Fiber::GetThis()->Yield();
    if (timer) {
      timer->Cancel();
    }
    if (tinfo->cancelled) {
      errno = tinfo->cancelled;
      return -1;
    }
  } else {
    if (timer) {
      timer->Cancel();
    }
    LOG_ERROR(g_logger) << "connect AddEvent(" << fd << ", WRITE) error";
  }

  int error = 0;
  socklen_t len = sizeof(int);
  if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
    return -1;
  }
  if (!error) {
    return 0;
  } else {
    errno = error;
    return -1;
  }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  return connect_with_timeout(sockfd, addr, addrlen,
                              serverframework::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
  int fd = do_io(s, accept_f, "accept", serverframework::IOManager::READ,
                 SO_RCVTIMEO, addr, addrlen);
  if (fd >= 0) {
    serverframework::FdMgr::GetInstance()->Get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
  return do_io(fd, read_f, "read", serverframework::IOManager::READ,
               SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, readv_f, "readv", serverframework::IOManager::READ,
               SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  return do_io(sockfd, recv_f, "recv", serverframework::IOManager::READ,
               SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
  return do_io(sockfd, recvfrom_f, "recvfrom", serverframework::IOManager::READ,
               SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  return do_io(sockfd, recvmsg_f, "recvmsg", serverframework::IOManager::READ,
               SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return do_io(fd, write_f, "write", serverframework::IOManager::WRITE,
               SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, writev_f, "writev", serverframework::IOManager::WRITE,
               SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
  return do_io(s, send_f, "send", serverframework::IOManager::WRITE,
               SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags,
               const struct sockaddr *to, socklen_t tolen) {
  return do_io(s, sendto_f, "sendto", serverframework::IOManager::WRITE,
               SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
  return do_io(s, sendmsg_f, "sendmsg", serverframework::IOManager::WRITE,
               SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
  if (!serverframework::t_hook_enable) {
    return close_f(fd);
  }

  serverframework::FdCtx::ptr ctx =
      serverframework::FdMgr::GetInstance()->Get(fd);
  if (ctx) {
    auto iom = serverframework::IOManager::GetThis();
    if (iom) {
      iom->CancelAll(fd);
    }
    serverframework::FdMgr::GetInstance()->Del(fd);
  }
  return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */) {
  va_list va;
  va_start(va, cmd);
  switch (cmd) {
    case F_SETFL: {
      int arg = va_arg(va, int);
      va_end(va);
      serverframework::FdCtx::ptr ctx =
          serverframework::FdMgr::GetInstance()->Get(fd);
      if (!ctx || ctx->IsClose() || !ctx->IsSocket()) {
        return fcntl_f(fd, cmd, arg);
      }
      ctx->SetUserNonblock(arg & O_NONBLOCK);
      if (ctx->GetSysNonblock()) {
        arg |= O_NONBLOCK;
      } else {
        arg &= ~O_NONBLOCK;
      }
      return fcntl_f(fd, cmd, arg);
    } break;
    case F_GETFL: {
      va_end(va);
      int arg = fcntl_f(fd, cmd);
      serverframework::FdCtx::ptr ctx =
          serverframework::FdMgr::GetInstance()->Get(fd);
      if (!ctx || ctx->IsClose() || !ctx->IsSocket()) {
        return arg;
      }
      if (ctx->GetUserNonblock()) {
        return arg | O_NONBLOCK;
      } else {
        return arg & ~O_NONBLOCK;
      }
    } break;
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
#endif
    {
      int arg = va_arg(va, int);
      va_end(va);
      return fcntl_f(fd, cmd, arg);
    } break;
    case F_GETFD:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
#ifdef F_GETPIPE_SZ
    case F_GETPIPE_SZ:
#endif
    {
      va_end(va);
      return fcntl_f(fd, cmd);
    } break;
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {
      struct flock *arg = va_arg(va, struct flock *);
      va_end(va);
      return fcntl_f(fd, cmd, arg);
    } break;
    case F_GETOWN_EX:
    case F_SETOWN_EX: {
      struct f_owner_exlock *arg = va_arg(va, struct f_owner_exlock *);
      va_end(va);
      return fcntl_f(fd, cmd, arg);
    } break;
    default:
      va_end(va);
      return fcntl_f(fd, cmd);
  }
}

int ioctl(int d, unsigned long int request, ...) {
  va_list va;
  va_start(va, request);
  void *arg = va_arg(va, void *);
  va_end(va);

  if (FIONBIO == request) {
    bool user_nonblock = !!*(int *)arg;
    serverframework::FdCtx::ptr ctx =
        serverframework::FdMgr::GetInstance()->Get(d);
    if (!ctx || ctx->IsClose() || !ctx->IsSocket()) {
      return ioctl_f(d, request, arg);
    }
    ctx->SetUserNonblock(user_nonblock);
  }
  return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval,
               socklen_t *optlen) {
  return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval,
               socklen_t optlen) {
  if (!serverframework::t_hook_enable) {
    return setsockopt_f(sockfd, level, optname, optval, optlen);
  }
  if (level == SOL_SOCKET) {
    if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
      serverframework::FdCtx::ptr ctx =
          serverframework::FdMgr::GetInstance()->Get(sockfd);
      if (ctx) {
        const timeval *v = (const timeval *)optval;
        ctx->SetTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
      }
    }
  }
  return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}
