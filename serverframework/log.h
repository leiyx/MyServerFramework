/**
 * @file Log.h
 * @brief 日志模块
 */

#ifndef LOG_H
#define LOG_H

#include <cstdarg>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mutex.h"
#include "singleton.h"
#include "util.h"

/**
 * @brief 获取root日志器
 */
#define LOG_ROOT() serverframework::LoggerMgr::GetInstance()->GetRoot()

/**
 * @brief 获取指定名称的日志器
 */
#define LOG_NAME(name) \
  serverframework::LoggerMgr::GetInstance()->GetLogger(name)

/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 * @details
 * 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
 */
#define LOG_LEVEL(logger, level)                                         \
  if (level <= logger->GetLevel())                                       \
  serverframework::LogEventWrap(                                         \
      logger,                                                            \
      serverframework::LogEvent::ptr(new serverframework::LogEvent(      \
          logger->GetName(), level, __FILE__, __LINE__,                  \
          serverframework::GetElapsedMS() - logger->GetCreateTime(),     \
          serverframework::GetThreadId(), serverframework::GetFiberId(), \
          time(0), serverframework::GetThreadName())))                   \
      .GetLogEvent()                                                     \
      ->GetSS()

#define LOG_FATAL(logger) LOG_LEVEL(logger, serverframework::LogLevel::FATAL)

#define LOG_ALERT(logger) LOG_LEVEL(logger, serverframework::LogLevel::ALERT)

#define LOG_CRIT(logger) LOG_LEVEL(logger, serverframework::LogLevel::CRIT)

#define LOG_ERROR(logger) LOG_LEVEL(logger, serverframework::LogLevel::ERROR)

#define LOG_WARN(logger) LOG_LEVEL(logger, serverframework::LogLevel::WARN)

#define LOG_NOTICE(logger) LOG_LEVEL(logger, serverframework::LogLevel::NOTICE)

#define LOG_INFO(logger) LOG_LEVEL(logger, serverframework::LogLevel::INFO)

#define LOG_DEBUG(logger) LOG_LEVEL(logger, serverframework::LogLevel::DEBUG)

/**
 * @brief 使用C printf方式将日志级别level的日志写入到logger
 * @details
 * 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
 * @todo 协程id未实现，暂时写0
 */
#define LOG_FMT_LEVEL(logger, level, fmt, ...)                           \
  if (level <= logger->GetLevel())                                       \
  serverframework::LogEventWrap(                                         \
      logger,                                                            \
      serverframework::LogEvent::ptr(new serverframework::LogEvent(      \
          logger->GetName(), level, __FILE__, __LINE__,                  \
          serverframework::GetElapsedMS() - logger->GetCreateTime(),     \
          serverframework::GetThreadId(), serverframework::GetFiberId(), \
          time(0), serverframework::GetThreadName())))                   \
      .GetLogEvent()                                                     \
      ->Printf(fmt, __VA_ARGS__)

#define LOG_FMT_FATAL(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::FATAL, fmt, __VA_ARGS__)

#define LOG_FMT_ALERT(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::ALERT, fmt, __VA_ARGS__)

#define LOG_FMT_CRIT(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::CRIT, fmt, __VA_ARGS__)

#define LOG_FMT_ERROR(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::ERROR, fmt, __VA_ARGS__)

#define LOG_FMT_WARN(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::WARN, fmt, __VA_ARGS__)

#define LOG_FMT_NOTICE(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::NOTICE, fmt, __VA_ARGS__)

#define LOG_FMT_INFO(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::INFO, fmt, __VA_ARGS__)

#define LOG_FMT_DEBUG(logger, fmt, ...) \
  LOG_FMT_LEVEL(logger, serverframework::LogLevel::DEBUG, fmt, __VA_ARGS__)

namespace serverframework {

/**
 * @brief 日志级别
 */
class LogLevel {
 public:
  /**
   * @brief 日志级别枚举，参考log4cpp
   */
  enum Level {
    // 致命情况，系统不可用
    FATAL = 0,
    // 高优先级情况，例如数据库系统崩溃
    ALERT = 100,
    // 严重错误，例如硬盘错误
    CRIT = 200,
    // 错误
    ERROR = 300,
    // 警告
    WARN = 400,
    // 正常但值得注意
    NOTICE = 500,
    // 一般信息
    INFO = 600,
    // 调试信息
    DEBUG = 700,
    // 未设置
    NOTSET = 800,
  };

  /**
   * @brief 日志级别转字符串
   * @param[in] level 日志级别
   * @return 字符串形式的日志级别
   */
  static const char *ToString(LogLevel::Level level);

  /**
   * @brief 字符串转日志级别
   * @param[in] str 字符串
   * @return 日志级别
   * @note 不区分大小写
   */
  static LogLevel::Level FromString(const std::string &str);
};

/**
 * @brief 日志事件
 */
class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;

  /**
   * @brief 构造函数
   * @param[in] logger_name 日志器名称
   * @param[in] level 日志级别
   * @param[in] file 文件名
   * @param[in] line 行号
   * @param[in] elapse 从日志器创建开始到当前的累计运行毫秒
   * @param[in] thead_id 线程id
   * @param[in] fiber_id 协程id
   * @param[in] time UTC时间
   * @param[in] thread_name 线程名称
   */
  LogEvent(const std::string &logger_name, LogLevel::Level level,
           const char *file, int32_t line, int64_t elapse, uint32_t thread_id,
           uint64_t fiber_id, time_t time, const std::string &thread_name);

  /**
   * @brief 获取日志级别
   */
  LogLevel::Level GetLevel() const { return level_; }

  /**
   * @brief 获取日志内容
   */
  std::string GetContent() const { return ss_.str(); }

  /**
   * @brief 获取文件名
   */
  std::string GetFile() const { return file_; }

  /**
   * @brief 获取行号
   */
  int32_t GetLine() const { return line_; }

  /**
   * @brief 获取累计运行毫秒数
   */
  int64_t GetElapse() const { return elapse_; }

  /**
   * @brief 获取线程id
   */
  uint32_t GetThreadId() const { return thread_id_; }

  /**
   * @brief 获取协程id
   */
  uint64_t GetFiberId() const { return fiber_id_; }

  /**
   * @brief 返回时间戳
   */
  time_t GetTime() const { return time_; }

  /**
   * @brief 获取线程名称
   */
  const std::string &GetThreadName() const { return thread_name_; }

  /**
   * @brief 获取内容字节流，用于流式写入日志
   */
  std::stringstream &GetSS() { return ss_; }

  /**
   * @brief 获取日志器名称
   */
  const std::string &GetLoggerName() const { return logger_name_; }

  /**
   * @brief C prinf风格写入日志
   */
  void Printf(const char *fmt, ...);

  /**
   * @brief C vprintf风格写入日志
   */
  void VPrintf(const char *fmt, va_list ap);

 private:
  // 日志级别
  LogLevel::Level level_;
  // 日志内容，使用stringstream存储，便于流式写入日志
  std::stringstream ss_;
  // 文件名
  const char *file_ = nullptr;
  // 行号
  int32_t line_ = 0;
  // 从日志器创建开始到当前的耗时
  int64_t elapse_ = 0;
  // 线程id
  uint32_t thread_id_ = 0;
  // 协程id
  uint64_t fiber_id_ = 0;
  // UTC时间戳
  time_t time_;
  // 线程名称
  std::string thread_name_;
  // 日志器名称
  std::string logger_name_;
};

/**
 * @brief 日志格式化
 */
class LogFormatter {
 public:
  using ptr = std::shared_ptr<LogFormatter>;

  /**
   * @brief 构造函数
   * @param[in] pattern 格式模板，参考sylar与log4cpp
   * @details 模板参数说明：
   * - %%m 消息
   * - %%p 日志级别
   * - %%c 日志器名称
   * - %%d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d
   * %%H:%%M:%%S}，这里的格式字符与C语言strftime一致
   * - %%r 该日志器创建后的累计运行毫秒数
   * - %%f 文件名
   * - %%l 行号
   * - %%t 线程id
   * - %%F 协程id
   * - %%N 线程名称
   * - %%% 百分号
   * - %%T 制表符
   * - %%n 换行
   *
   * 默认格式：%%d{%%Y-%%m-%%d
   * %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
   *
   * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \\t 线程id \\t 线程名称
   * \\t 协程id \\t [日志级别] \\t [日志器名称] \\t 文件名:行号 \\t 日志消息
   * 换行符
   */
  LogFormatter(
      const std::string &pattern =
          "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

  /**
   * @brief 初始化，解析格式模板，提取模板项
   */
  void Init();

  /**
   * @brief 模板解析是否出错
   */
  bool IsError() const { return error_; }

  /**
   * @brief 对日志事件进行格式化，返回格式化日志文本
   * @param[in] event 日志事件
   * @return 格式化日志字符串
   */
  std::string Format(LogEvent::ptr event);

  /**
   * @brief 对日志事件进行格式化，返回格式化日志流
   * @param[in] event 日志事件
   * @param[in] os 日志输出流
   * @return 格式化日志流
   */
  std::ostream &Format(std::ostream &os, LogEvent::ptr event);

  /**
   * @brief 获取pattern
   */
  std::string GetPattern() const { return pattern_; }

 public:
  /**
   * @brief 日志内容格式化项，虚基类，用于派生出不同的格式化项
   */
  class FormatItem {
   public:
    using ptr = std::shared_ptr<FormatItem>;

    /**
     * @brief 析构函数
     */
    virtual ~FormatItem() {}

    /**
     * @brief 格式化日志事件
     */
    virtual void Format(std::ostream &os, LogEvent::ptr event) = 0;
  };

 private:
  // 日志格式模板
  std::string pattern_;
  // 解析后的格式模板数组
  std::vector<FormatItem::ptr> items_;
  // 是否出错
  bool error_ = false;
};

/**
 * @brief 日志输出地，虚基类，用于派生出不同的LogAppender
 * @details 参考log4cpp，Appender自带一个默认的LogFormatter，以控件默认输出格式
 */
class LogAppender {
 public:
  using ptr = std::shared_ptr<LogAppender>;
  using MutexType = Spinlock;

  /**
   * @brief 构造函数
   * @param[in] default_formatter 默认日志格式器
   */
  LogAppender(LogFormatter::ptr default_formatter);

  /**
   * @brief 析构函数
   */
  virtual ~LogAppender() {}

  /**
   * @brief 设置日志格式器
   */
  void SetFormatter(LogFormatter::ptr val);

  /**
   * @brief 获取日志格式器
   */
  LogFormatter::ptr GetFormatter();

  /**
   * @brief 写入日志
   */
  virtual void Log(LogEvent::ptr event) = 0;

  /**
   * @brief 将日志输出目标的配置转成YAML String
   */
  virtual std::string ToYamlString() = 0;

 protected:
  // Mutex
  MutexType mutex_;
  // 日志格式器
  LogFormatter::ptr formatter_;
  // 默认日志格式器
  LogFormatter::ptr default_formatter_;
};

/**
 * @brief 输出到控制台的Appender
 */
class StdoutLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<StdoutLogAppender>;

  /**
   * @brief 构造函数
   */
  StdoutLogAppender();

  /**
   * @brief 写入日志
   */
  void Log(LogEvent::ptr event) override;

  /**
   * @brief 将日志输出目标的配置转成YAML String
   */
  std::string ToYamlString() override;
};

/**
 * @brief 输出到文件
 */
class FileLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<FileLogAppender>;

  /**
   * @brief 构造函数
   * @param[in] file 日志文件路径
   */
  FileLogAppender(const std::string &file);

  /**
   * @brief 写日志
   */
  void Log(LogEvent::ptr event) override;

  /**
   * @brief 重新打开日志文件
   * @return 成功返回true
   */
  bool Reopen();

  /**
   * @brief 将日志输出目标的配置转成YAML String
   */
  std::string ToYamlString() override;

 private:
  // 文件路径
  std::string file_name_;
  // 文件流
  std::ofstream file_stream_;
  // 上次重打打开时间
  uint64_t last_time_ = 0;
  // 文件打开错误标识
  bool reopen_error_ = false;
};

/**
 * @brief 日志器类
 * @note 日志器类不带root logger
 */
class Logger {
 public:
  using ptr = std::shared_ptr<Logger>;
  using MutexType = Spinlock;

  /**
   * @brief 构造函数
   * @param[in] name 日志器名称
   */
  Logger(const std::string &name = "default");

  /**
   * @brief 获取日志器名称
   */
  const std::string &GetName() const { return name_; }

  /**
   * @brief 获取创建时间
   */
  const uint64_t &GetCreateTime() const { return create_time_; }

  /**
   * @brief 设置日志级别
   */
  void SetLevel(LogLevel::Level level) { level_ = level; }

  /**
   * @brief 获取日志级别
   */
  LogLevel::Level GetLevel() const { return level_; }

  /**
   * @brief 添加LogAppender
   */
  void AddAppender(LogAppender::ptr appender);

  /**
   * @brief 删除LogAppender
   */
  void DelAppender(LogAppender::ptr appender);

  /**
   * @brief 清空LogAppender
   */
  void ClearAppenders();

  /**
   * @brief 写日志
   */
  void Log(LogEvent::ptr event);

  /**
   * @brief 将日志器的配置转成YAML String
   */
  std::string ToYamlString();

 private:
  // Mutex
  MutexType mutex_;
  // 日志器名称
  std::string name_;
  // 日志器等级
  LogLevel::Level level_;
  // LogAppender集合
  std::list<LogAppender::ptr> appenders_;
  // 创建时间（毫秒）
  uint64_t create_time_;
};

/**
 * @brief 日志事件包装器，方便宏定义，内部包含日志事件和日志器
 */
class LogEventWrap {
 public:
  /**
   * @brief 构造函数
   * @param[in] logger 日志器
   * @param[in] event 日志事件
   */
  LogEventWrap(Logger::ptr logger, LogEvent::ptr event);

  /**
   * @brief 析构函数
   * @details 日志事件在析构时由日志器进行输出
   */
  ~LogEventWrap();

  /**
   * @brief 获取日志事件
   */
  LogEvent::ptr GetLogEvent() const { return event_; }

 private:
  // 日志器
  Logger::ptr logger_;
  // 日志事件
  LogEvent::ptr event_;
};

/**
 * @brief 日志器管理类
 */
class LoggerManager {
 public:
  using MutexType = Spinlock;

  /**
   * @brief 构造函数
   */
  LoggerManager();

  /**
   * @brief 初始化，主要是结合配置模块实现日志模块初始化
   */
  void Init();

  /**
   * @brief 获取指定名称的日志器
   */
  Logger::ptr GetLogger(const std::string &name);

  /**
   * @brief 获取root日志器，等效于getLogger("root")
   */
  Logger::ptr GetRoot() { return root_; }

  /**
   * @brief 将所有的日志器配置转成YAML String
   */
  std::string ToYamlString();

 private:
  // Mutex
  MutexType mutex_;
  // 日志器集合
  std::map<std::string, Logger::ptr> loggers_;
  // root日志器
  Logger::ptr root_;
};

// 日志器管理类单例
using LoggerMgr = serverframework::Singleton<LoggerManager>;

}  // end namespace serverframework

#endif  // LOG_H
