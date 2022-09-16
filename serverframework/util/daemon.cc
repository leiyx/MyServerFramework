/**
 * @file daemon.cc
 * @brief 守护进程启动实现
 */
#include "util/daemon.h"

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "config/config.h"
#include "log/log.h"

namespace serverframework {

static serverframework::Logger::ptr g_logger = LOG_NAME("system");
static serverframework::ConfigVar<uint32_t>::ptr g_daemon_restart_interval =
    serverframework::Config::Lookup("daemon.restart_interval", (uint32_t)5,
                                    "daemon restart interval");

std::string ProcessInfo::ToString() const {
  std::stringstream ss;
  ss << "[ProcessInfo parent_id_=" << parent_id_ << " main_id_=" << main_id_
     << " parent_start_time_=" << serverframework::Time2Str(parent_start_time_)
     << " main_start_time=" << serverframework::Time2Str(main_start_time)
     << " restart_count_=" << restart_count_ << "]";
  return ss.str();
}

static int RealStart(int argc, char** argv,
                     std::function<int(int argc, char** argv)> main_cb) {
  return main_cb(argc, argv);
}

static int RealDaemon(int argc, char** argv,
                      std::function<int(int argc, char** argv)> main_cb) {
  daemon(1, 0);
  ProcessInfoMgr::GetInstance()->parent_id_ = getpid();
  ProcessInfoMgr::GetInstance()->parent_start_time_ = time(0);
  while (true) {
    pid_t pid = fork();
    if (pid == 0) {
      //子进程返回
      ProcessInfoMgr::GetInstance()->main_id_ = getpid();
      ProcessInfoMgr::GetInstance()->main_start_time = time(0);
      LOG_INFO(g_logger) << "process Start pid=" << getpid();
      return RealStart(argc, argv, main_cb);
    } else if (pid < 0) {
      LOG_ERROR(g_logger) << "fork fail return=" << pid << " errno=" << errno
                          << " errstr=" << strerror(errno);
      return -1;
    } else {
      //父进程返回
      int status = 0;
      waitpid(pid, &status, 0);
      if (status) {
        LOG_ERROR(g_logger)
            << "child crash pid=" << pid << " status=" << status;
      } else {
        LOG_INFO(g_logger) << "child finished pid=" << pid;
        break;
      }
      ProcessInfoMgr::GetInstance()->restart_count_ += 1;
      sleep(g_daemon_restart_interval->GetValue());
    }
  }
  return 0;
}

int StartDaemon(int argc, char** argv,
                std::function<int(int argc, char** argv)> main_cb,
                bool is_daemon) {
  if (!is_daemon) {
    return RealStart(argc, argv, main_cb);
  }
  return RealDaemon(argc, argv, main_cb);
}

}  // namespace serverframework
