/**
 * @file test_util.cpp
 * @brief util与macro测试
 */
#include "serverframework/serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

void test2() { std::cout << serverframework::BacktraceToString() << std::endl; }
void test1() { test2(); }

void test_backtrace() { test1(); }

int main() {
  LOG_INFO(g_logger) << serverframework::GetCurrentMS();
  LOG_INFO(g_logger) << serverframework::GetCurrentUS();
  LOG_INFO(g_logger) << serverframework::ToUpper("hello");
  LOG_INFO(g_logger) << serverframework::ToLower("HELLO");
  LOG_INFO(g_logger) << serverframework::Time2Str();
  LOG_INFO(g_logger) << serverframework::Str2Time(
      "1970-01-01 00:00:00");  // -28800

  std::vector<std::string> files;
  serverframework::FSUtil::ListAllFile(files, "./serverframework", ".cpp");
  for (auto &i : files) {
    LOG_INFO(g_logger) << i;
  }

  // todo, more...

  test_backtrace();

  ASSERT2(false, "assert");
  return 0;
}