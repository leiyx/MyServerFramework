/**
 * @file test_log.cpp
 * @brief 日志类测试
 */

#include <unistd.h>

#include "serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();  // 默认INFO级别

int main(int argc, char *argv[]) {
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir(
      serverframework::EnvMgr::GetInstance()->GetConfigPath());

  LOG_FATAL(g_logger) << "fatal msg";
  LOG_ERROR(g_logger) << "err msg";
  LOG_INFO(g_logger) << "info msg";
  LOG_DEBUG(g_logger) << "debug msg";

  LOG_FMT_FATAL(g_logger, "fatal %s:%d", __FILE__, __LINE__);
  LOG_FMT_ERROR(g_logger, "err %s:%d", __FILE__, __LINE__);
  LOG_FMT_INFO(g_logger, "info %s:%d", __FILE__, __LINE__);
  LOG_FMT_DEBUG(g_logger, "debug %s:%d", __FILE__, __LINE__);

  sleep(1);
  serverframework::SetThreadName("brand_new_thread");

  g_logger->SetLevel(serverframework::LogLevel::WARN);
  LOG_FATAL(g_logger) << "fatal msg";
  LOG_ERROR(g_logger) << "err msg";
  LOG_INFO(g_logger) << "info msg";    // 不打印
  LOG_DEBUG(g_logger) << "debug msg";  // 不打印

  serverframework::FileLogAppender::ptr fileAppender(
      new serverframework::FileLogAppender("./log.txt"));
  g_logger->AddAppender(fileAppender);
  LOG_FATAL(g_logger) << "fatal msg";
  LOG_ERROR(g_logger) << "err msg";
  LOG_INFO(g_logger) << "info msg";    // 不打印
  LOG_DEBUG(g_logger) << "debug msg";  // 不打印

  serverframework::Logger::ptr test_logger = LOG_NAME("test_logger");
  serverframework::StdoutLogAppender::ptr appender(
      new serverframework::StdoutLogAppender);
  serverframework::LogFormatter::ptr formatter(
      new serverframework::LogFormatter(
          "%d:%rms%T%p%T%c%T%f:%l %m%n"));  // 时间：启动毫秒数 级别 日志名称
                                            // 文件名：行号 消息 换行
  appender->SetFormatter(formatter);
  test_logger->AddAppender(appender);
  test_logger->SetLevel(serverframework::LogLevel::WARN);

  LOG_ERROR(test_logger) << "err msg";
  LOG_INFO(test_logger) << "info msg";  // 不打印

  // 输出全部日志器的配置
  g_logger->SetLevel(serverframework::LogLevel::INFO);
  LOG_INFO(g_logger)
      << "logger config:"
      << serverframework::LoggerMgr::GetInstance()->ToYamlString();

  return 0;
}