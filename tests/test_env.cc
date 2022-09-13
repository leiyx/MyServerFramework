/**
 * @file test_env.cc
 * @brief 环境变量测试
 */
#include "serverframework/serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

serverframework::Env *g_env = serverframework::EnvMgr::GetInstance();

int main(int argc, char *argv[]) {
  g_env->AddHelp("h", "print this help message");

  bool is_print_help = false;
  if (!g_env->Init(argc, argv)) {
    is_print_help = true;
  }
  if (g_env->Has("h")) {
    is_print_help = true;
  }

  if (is_print_help) {
    g_env->PrintHelp();
    return false;
  }

  LOG_INFO(g_logger) << "exe: " << g_env->GetExe();
  LOG_INFO(g_logger) << "cwd: " << g_env->GetCwd();
  LOG_INFO(g_logger) << "absoluth path of test: "
                     << g_env->GetAbsolutePath("test");
  LOG_INFO(g_logger) << "conf path:" << g_env->GetConfigPath();

  g_env->Add("key1", "value1");
  LOG_INFO(g_logger) << "key1: " << g_env->Get("key1");

  g_env->SetEnv("key2", "value2");
  LOG_INFO(g_logger) << "key2: " << g_env->GetEnv("key2");

  LOG_INFO(g_logger) << g_env->GetEnv("PATH");

  return 0;
}