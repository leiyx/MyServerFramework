/**
 * @file log.cpp
 * @brief 日志模块实现
 */

#include "log/log.h"

#include <utility>  // for std::pair

#include "config/config.h"
#include "env/env.h"

namespace serverframework {

const char *LogLevel::ToString(LogLevel::Level level) {
  switch (level) {
#define XX(name)       \
  case LogLevel::name: \
    return #name;
    XX(FATAL);
    XX(ALERT);
    XX(CRIT);
    XX(ERROR);
    XX(WARN);
    XX(NOTICE);
    XX(INFO);
    XX(DEBUG);
#undef XX
    default:
      return "NOTSET";
  }
  return "NOTSET";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(level, v)        \
  if (str == #v) {          \
    return LogLevel::level; \
  }
  XX(FATAL, fatal);
  XX(ALERT, alert);
  XX(CRIT, crit);
  XX(ERROR, error);
  XX(WARN, warn);
  XX(NOTICE, notice);
  XX(INFO, info);
  XX(DEBUG, debug);

  XX(FATAL, FATAL);
  XX(ALERT, ALERT);
  XX(CRIT, CRIT);
  XX(ERROR, ERROR);
  XX(WARN, WARN);
  XX(NOTICE, NOTICE);
  XX(INFO, INFO);
  XX(DEBUG, DEBUG);
#undef XX

  return LogLevel::NOTSET;
}

LogEvent::LogEvent(const std::string &logger_name, LogLevel::Level level,
                   const char *file, int32_t line, int64_t elapse,
                   uint32_t thread_id, uint64_t fiber_id, time_t time,
                   const std::string &thread_name)
    : level_(level),
      file_(file),
      line_(line),
      elapse_(elapse),
      thread_id_(thread_id),
      fiber_id_(fiber_id),
      time_(time),
      thread_name_(thread_name),
      logger_name_(logger_name) {}

void LogEvent::Printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VPrintf(fmt, ap);
  va_end(ap);
}

void LogEvent::VPrintf(const char *fmt, va_list ap) {
  char *buf = nullptr;
  int len = vasprintf(&buf, fmt, ap);
  if (len != -1) {
    ss_ << std::string(buf, len);
    free(buf);
  }
}

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  MessageFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetContent();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  LevelFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << LogLevel::ToString(event->GetLevel());
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  ElapseFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetElapse();
  }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
 public:
  LoggerNameFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetLoggerName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadIdFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetThreadId();
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  FiberIdFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetFiberId();
  }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadNameFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetThreadName();
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  DateTimeFormatItem(const std::string &Format = "%Y-%m-%d %H:%M:%S")
      : m_format(Format) {
    if (m_format.empty()) {
      m_format = "%Y-%m-%d %H:%M:%S";
    }
  }

  void Format(std::ostream &os, LogEvent::ptr event) override {
    struct tm tm;
    time_t time = event->GetTime();
    localtime_r(&time, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), m_format.c_str(), &tm);
    os << buf;
  }

 private:
  std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
 public:
  FileNameFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetFile();
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  LineFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << event->GetLine();
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  NewLineFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << std::endl;
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  StringFormatItem(const std::string &str) : m_string(str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override {
    os << m_string;
  }

 private:
  std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  TabFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override { os << "\t"; }
};

class PercentSignFormatItem : public LogFormatter::FormatItem {
 public:
  PercentSignFormatItem(const std::string &str) {}
  void Format(std::ostream &os, LogEvent::ptr event) override { os << "%"; }
};

LogFormatter::LogFormatter(const std::string &pattern) : pattern_(pattern) {
  Init();
}

/**
 * 简单的状态机判断，提取pattern中的常规字符和模式字符
 *
 * 解析的过程就是从头到尾遍历，根据状态标志决定当前字符是常规字符还是模式字符
 *
 * 一共有两种状态，即正在解析常规字符和正在解析模板转义字符
 *
 * 比较麻烦的是%%d，后面可以接一对大括号指定时间格式，比如%%d{%%Y-%%m-%%d
 * %%H:%%M:%%S}，这个状态需要特殊处理
 *
 * 一旦状态出错就停止解析，并设置错误标志，未识别的pattern转义字符也算出错
 *
 * @see LogFormatter::LogFormatter
 */
void LogFormatter::Init() {
  // 按顺序存储解析到的pattern项
  // 每个pattern包括一个整数类型和一个字符串，类型为0表示该pattern是常规字符串，为1表示该pattern需要转义
  // 日期格式单独用下面的dataformat存储
  std::vector<std::pair<int, std::string>> patterns;
  // 临时存储常规字符串
  std::string tmp;
  // 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
  std::string dateformat;
  // 是否解析出错
  bool error = false;

  // 是否正在解析常规字符，初始时为true
  bool parsing_string = true;
  // 是否正在解析模板字符，%后面的是模板字符
  // bool parsing_pattern = false;

  size_t i = 0;
  while (i < pattern_.size()) {
    std::string c = std::string(1, pattern_[i]);
    if (c == "%") {
      if (parsing_string) {
        if (!tmp.empty()) {
          patterns.push_back(std::make_pair(0, tmp));
        }
        tmp.clear();
        parsing_string = false;  // 在解析常规字符时遇到%，表示开始解析模板字符
        // parsing_pattern = true;
        i++;
        continue;
      } else {
        patterns.push_back(std::make_pair(1, c));
        parsing_string = true;  // 在解析模板字符时遇到%，表示这里是一个%转义
        // parsing_pattern = false;
        i++;
        continue;
      }
    } else {                 // not %
      if (parsing_string) {  // 持续解析常规字符直到遇到%，解析出的字符串作为一个常规字符串加入patterns
        tmp += c;
        i++;
        continue;
      } else {  // 模板字符，直接添加到patterns中，添加完成后，状态变为解析常规字符，%d特殊处理
        patterns.push_back(std::make_pair(1, c));
        parsing_string = true;
        // parsing_pattern = false;

        // 后面是对%d的特殊处理，如果%d后面直接跟了一对大括号，那么把大括号里面的内容提取出来作为dateformat
        if (c != "d") {
          i++;
          continue;
        }
        i++;
        if (i < pattern_.size() && pattern_[i] != '{') {
          continue;
        }
        i++;
        while (i < pattern_.size() && pattern_[i] != '}') {
          dateformat.push_back(pattern_[i]);
          i++;
        }
        if (pattern_[i] != '}') {
          // %d后面的大括号没有闭合，直接报错
          std::cout << "[ERROR] LogFormatter::Init() "
                    << "pattern: [" << pattern_ << "] '{' not closed"
                    << std::endl;
          error = true;
          break;
        }
        i++;
        continue;
      }
    }
  }  // end while(i < pattern_.size())

  if (error) {
    error_ = true;
    return;
  }

  // 模板解析结束之后剩余的常规字符也要算进去
  if (!tmp.empty()) {
    patterns.push_back(std::make_pair(0, tmp));
    tmp.clear();
  }

  // for debug
  // std::cout << "patterns:" << std::endl;
  // for(auto &v : patterns) {
  //     std::cout << "type = " << v.first << ", value = " << v.second <<
  //     std::endl;
  // }
  // std::cout << "dataformat = " << dateformat << std::endl;

  static std::map<std::string,
                  std::function<FormatItem::ptr(const std::string &str)>>
      s_format_items = {
#define XX(str, C)                                                           \
  {                                                                          \
#str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); } \
  }

          XX(m, MessageFormatItem),     // m:消息
          XX(p, LevelFormatItem),       // p:日志级别
          XX(c, LoggerNameFormatItem),  // c:日志器名称
          //        XX(d, DateTimeFormatItem),          // d:日期时间
          XX(r, ElapseFormatItem),       // r:累计毫秒数
          XX(f, FileNameFormatItem),     // f:文件名
          XX(l, LineFormatItem),         // l:行号
          XX(t, ThreadIdFormatItem),     // t:编程号
          XX(F, FiberIdFormatItem),      // F:协程号
          XX(N, ThreadNameFormatItem),   // N:线程名称
          XX(%, PercentSignFormatItem),  // %:百分号
          XX(T, TabFormatItem),          // T:制表符
          XX(n, NewLineFormatItem),      // n:换行符
#undef XX
      };

  for (auto &v : patterns) {
    if (v.first == 0) {
      items_.push_back(FormatItem::ptr(new StringFormatItem(v.second)));
    } else if (v.second == "d") {
      items_.push_back(FormatItem::ptr(new DateTimeFormatItem(dateformat)));
    } else {
      auto it = s_format_items.find(v.second);
      if (it == s_format_items.end()) {
        std::cout << "[ERROR] LogFormatter::Init() "
                  << "pattern: [" << pattern_ << "] "
                  << "unknown Format item: " << v.second << std::endl;
        error = true;
        break;
      } else {
        items_.push_back(it->second(v.second));
      }
    }
  }

  if (error) {
    error_ = true;
    return;
  }
}

std::string LogFormatter::Format(LogEvent::ptr event) {
  std::stringstream ss;
  for (auto &i : items_) {
    i->Format(ss, event);
  }
  return ss.str();
}

std::ostream &LogFormatter::Format(std::ostream &os, LogEvent::ptr event) {
  for (auto &i : items_) {
    i->Format(os, event);
  }
  return os;
}

LogAppender::LogAppender(LogFormatter::ptr default_formatter)
    : default_formatter_(default_formatter) {}

void LogAppender::SetFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(mutex_);
  formatter_ = val;
}

LogFormatter::ptr LogAppender::GetFormatter() {
  MutexType::Lock lock(mutex_);
  return formatter_ ? formatter_ : default_formatter_;
}

StdoutLogAppender::StdoutLogAppender()
    : LogAppender(LogFormatter::ptr(new LogFormatter)) {}

void StdoutLogAppender::Log(LogEvent::ptr event) {
  if (formatter_) {
    formatter_->Format(std::cout, event);
  } else {
    default_formatter_->Format(std::cout, event);
  }
}

std::string StdoutLogAppender::ToYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  node["pattern"] = formatter_->GetPattern();
  std::stringstream ss;
  ss << node;
  return ss.str();
}

FileLogAppender::FileLogAppender(const std::string &file)
    : LogAppender(LogFormatter::ptr(new LogFormatter)) {
  file_name_ = file;
  Reopen();
  if (reopen_error_) {
    std::cout << "Reopen file " << file_name_ << " error" << std::endl;
  }
}

/**
 * 如果一个日志事件距离上次写日志超过3秒，那就重新打开一次日志文件
 */
void FileLogAppender::Log(LogEvent::ptr event) {
  uint64_t now = event->GetTime();
  if (now >= (last_time_ + 3)) {
    Reopen();
    if (reopen_error_) {
      std::cout << "Reopen file " << file_name_ << " error" << std::endl;
    }
    last_time_ = now;
  }
  if (reopen_error_) {
    return;
  }
  MutexType::Lock lock(mutex_);
  if (formatter_) {
    if (!formatter_->Format(file_stream_, event)) {
      std::cout << "[ERROR] FileLogAppender::log() Format error" << std::endl;
    }
  } else {
    if (!default_formatter_->Format(file_stream_, event)) {
      std::cout << "[ERROR] FileLogAppender::log() Format error" << std::endl;
    }
  }
}

bool FileLogAppender::Reopen() {
  MutexType::Lock lock(mutex_);
  if (file_stream_) {
    file_stream_.close();
  }
  file_stream_.open(file_name_, std::ios::app);
  reopen_error_ = !file_stream_;
  return !reopen_error_;
}

std::string FileLogAppender::ToYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = file_name_;
  node["pattern"] =
      formatter_ ? formatter_->GetPattern() : default_formatter_->GetPattern();
  std::stringstream ss;
  ss << node;
  return ss.str();
}

Logger::Logger(const std::string &name)
    : name_(name), level_(LogLevel::INFO), create_time_(GetElapsedMS()) {}

void Logger::AddAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  appenders_.push_back(appender);
}

void Logger::DelAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); it++) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

void Logger::ClearAppenders() {
  MutexType::Lock lock(mutex_);
  appenders_.clear();
}

/**
 * 调用Logger的所有appenders将日志写一遍，
 * Logger至少要有一个appender，否则没有输出
 */
void Logger::Log(LogEvent::ptr event) {
  if (event->GetLevel() <= level_) {
    for (auto &i : appenders_) {
      i->Log(event);
    }
  }
}

std::string Logger::ToYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  node["name"] = name_;
  node["level"] = LogLevel::ToString(level_);
  for (auto &i : appenders_) {
    node["appenders"].push_back(YAML::Load(i->ToYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event)
    : logger_(logger), event_(event) {}

/**
 * @note LogEventWrap在析构时写日志
 */
LogEventWrap::~LogEventWrap() { logger_->Log(event_); }

LoggerManager::LoggerManager() {
  root_.reset(new Logger("root"));
  root_->AddAppender(LogAppender::ptr(new StdoutLogAppender));
  loggers_[root_->GetName()] = root_;
  Init();
}

/**
 * 如果指定名称的日志器未找到，那会就新创建一个，但是新创建的Logger是不带Appender的，
 * 需要手动添加Appender
 */
Logger::ptr LoggerManager::GetLogger(const std::string &name) {
  MutexType::Lock lock(mutex_);
  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }

  Logger::ptr logger(new Logger(name));
  loggers_[name] = logger;
  return logger;
}

/**
 * @todo 实现从配置文件加载日志配置
 */
void LoggerManager::Init() {}

std::string LoggerManager::ToYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  for (auto &i : loggers_) {
    node.push_back(YAML::Load(i.second->ToYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////
// 从配置文件中加载日志配置
/**
 * @brief 日志输出器配置结构体定义
 */
struct LogAppenderDefine {
  int type = 0;  // 1 File, 2 Stdout
  std::string pattern;
  std::string file;

  bool operator==(const LogAppenderDefine &oth) const {
    return type == oth.type && pattern == oth.pattern && file == oth.file;
  }
};

/**
 * @brief 日志器配置结构体定义
 */
struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::NOTSET;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine &oth) const {
    return name == oth.name && level == oth.level && appenders == appenders;
  }

  bool operator<(const LogDefine &oth) const { return name < oth.name; }

  bool IsValid() const { return !name.empty(); }
};

template <>
class LexicalCast<std::string, LogDefine> {
 public:
  LogDefine operator()(const std::string &v) {
    YAML::Node n = YAML::Load(v);
    LogDefine ld;
    if (!n["name"].IsDefined()) {
      std::cout << "log config error: name is null, " << n << std::endl;
      throw std::logic_error("log config name is null");
    }
    ld.name = n["name"].as<std::string>();
    ld.level = LogLevel::FromString(
        n["level"].IsDefined() ? n["level"].as<std::string>() : "");

    if (n["appenders"].IsDefined()) {
      for (size_t i = 0; i < n["appenders"].size(); i++) {
        auto a = n["appenders"][i];
        if (!a["type"].IsDefined()) {
          std::cout << "log appender config error: appender type is null, " << a
                    << std::endl;
          continue;
        }
        std::string type = a["type"].as<std::string>();
        LogAppenderDefine lad;
        if (type == "FileLogAppender") {
          lad.type = 1;
          if (!a["file"].IsDefined()) {
            std::cout
                << "log appender config error: file appender file is null, "
                << a << std::endl;
            continue;
          }
          lad.file = a["file"].as<std::string>();
          if (a["pattern"].IsDefined()) {
            lad.pattern = a["pattern"].as<std::string>();
          }
        } else if (type == "StdoutLogAppender") {
          lad.type = 2;
          if (a["pattern"].IsDefined()) {
            lad.pattern = a["pattern"].as<std::string>();
          }
        } else {
          std::cout << "log appender config error: appender type is invalid, "
                    << a << std::endl;
          continue;
        }
        ld.appenders.push_back(lad);
      }
    }  // end for
    return ld;
  }
};

template <>
class LexicalCast<LogDefine, std::string> {
 public:
  std::string operator()(const LogDefine &i) {
    YAML::Node n;
    n["name"] = i.name;
    n["level"] = LogLevel::ToString(i.level);
    for (auto &a : i.appenders) {
      YAML::Node na;
      if (a.type == 1) {
        na["type"] = "FileLogAppender";
        na["file"] = a.file;
      } else if (a.type == 2) {
        na["type"] = "StdoutLogAppender";
      }
      if (!a.pattern.empty()) {
        na["pattern"] = a.pattern;
      }
      n["appenders"].push_back(na);
    }
    std::stringstream ss;
    ss << n;
    return ss.str();
  }
};

serverframework::ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
    serverframework::Config::Lookup("logs", std::set<LogDefine>(),
                                    "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->AddListener([](const std::set<LogDefine> &old_value,
                                  const std::set<LogDefine> &new_value) {
      LOG_INFO(LOG_ROOT()) << "on log config changed";
      for (auto &i : new_value) {
        auto it = old_value.find(i);
        serverframework::Logger::ptr logger;
        if (it == old_value.end()) {
          // 新增logger
          logger = LOG_NAME(i.name);
        } else {
          if (!(i == *it)) {
            // 修改的logger
            logger == LOG_NAME(i.name);
          } else {
            continue;
          }
        }
        logger->SetLevel(i.level);
        logger->ClearAppenders();
        for (auto &a : i.appenders) {
          serverframework::LogAppender::ptr ap;
          if (a.type == 1) {
            ap.reset(new FileLogAppender(a.file));
          } else if (a.type == 2) {
            // 如果以daemon方式运行，则不需要创建终端appender
            if (!serverframework::EnvMgr::GetInstance()->Has("d")) {
              ap.reset(new StdoutLogAppender);
            } else {
              continue;
            }
          }
          if (!a.pattern.empty()) {
            ap->SetFormatter(LogFormatter::ptr(new LogFormatter(a.pattern)));
          } else {
            ap->SetFormatter(LogFormatter::ptr(new LogFormatter));
          }
          logger->AddAppender(ap);
        }
      }

      // 以配置文件为主，如果程序里定义了配置文件中未定义的logger，那么把程序里定义的logger设置成无效
      for (auto &i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          auto logger = LOG_NAME(i.name);
          logger->SetLevel(LogLevel::NOTSET);
          logger->ClearAppenders();
        }
      }
    });
  }
};

//在main函数之前注册配置更改的回调函数
//用于在更新配置时将log相关的配置加载到Config
static LogIniter __log_init;

///////////////////////////////////////////////////////////////////////////////

}  // end namespace serverframework