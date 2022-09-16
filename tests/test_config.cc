/**
 * @file test_config.cc
 * @brief 配置模块测试
 */

#include "serverframework.h"

serverframework::Logger::ptr g_logger = LOG_ROOT();

serverframework::ConfigVar<int>::ptr g_int =
    serverframework::Config::Lookup("global.int", (int)8080, "global int");

serverframework::ConfigVar<float>::ptr g_float =
    serverframework::Config::Lookup("global.float", (float)10.2f,
                                    "global float");

// 字符串需显示构造，不能传字符串常量
serverframework::ConfigVar<std::string>::ptr g_string =
    serverframework::Config::Lookup("global.string", std::string("helloworld"),
                                    "global string");

serverframework::ConfigVar<std::vector<int>>::ptr g_int_vec =
    serverframework::Config::Lookup("global.int_vec", std::vector<int>{1, 2, 3},
                                    "global int vec");

serverframework::ConfigVar<std::list<int>>::ptr g_int_list =
    serverframework::Config::Lookup("global.int_list", std::list<int>{1, 2, 3},
                                    "global int list");

serverframework::ConfigVar<std::set<int>>::ptr g_int_set =
    serverframework::Config::Lookup("global.int_set", std::set<int>{1, 2, 3},
                                    "global int set");

serverframework::ConfigVar<std::unordered_set<int>>::ptr g_int_unordered_set =
    serverframework::Config::Lookup("global.int_unordered_set",
                                    std::unordered_set<int>{1, 2, 3},
                                    "global int unordered_set");

serverframework::ConfigVar<std::map<std::string, int>>::ptr g_map_string2int =
    serverframework::Config::Lookup(
        "global.map_string2int",
        std::map<std::string, int>{{"key1", 1}, {"key2", 2}},
        "global map string2int");

serverframework::ConfigVar<std::unordered_map<std::string, int>>::ptr
    g_unordered_map_string2int = serverframework::Config::Lookup(
        "global.unordered_map_string2int",
        std::unordered_map<std::string, int>{{"key1", 1}, {"key2", 2}},
        "global unordered_map string2int");

////////////////////////////////////////////////////////////
// 自定义配置
class Person {
 public:
  Person(){};
  std::string name_;
  int m_age = 0;
  bool m_sex = 0;

  std::string ToString() const {
    std::stringstream ss;
    ss << "[Person name=" << name_ << " age=" << m_age << " sex=" << m_sex
       << "]";
    return ss.str();
  }

  bool operator==(const Person &oth) const {
    return name_ == oth.name_ && m_age == oth.m_age && m_sex == oth.m_sex;
  }
};

// 实现自定义配置的YAML序列化与反序列化，这部分要放在sylar命名空间中
namespace serverframework {

template <>
class LexicalCast<std::string, Person> {
 public:
  Person operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    Person p;
    p.name_ = node["name"].as<std::string>();
    p.m_age = node["age"].as<int>();
    p.m_sex = node["sex"].as<bool>();
    return p;
  }
};

template <>
class LexicalCast<Person, std::string> {
 public:
  std::string operator()(const Person &p) {
    YAML::Node node;
    node["name"] = p.name_;
    node["age"] = p.m_age;
    node["sex"] = p.m_sex;
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

}  // end namespace serverframework

serverframework::ConfigVar<Person>::ptr g_person =
    serverframework::Config::Lookup("class.person", Person(), "system person");

serverframework::ConfigVar<std::map<std::string, Person>>::ptr g_person_map =
    serverframework::Config::Lookup(
        "class.map", std::map<std::string, Person>(), "system person map");

serverframework::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr
    g_person_vec_map = serverframework::Config::Lookup(
        "class.vec_map", std::map<std::string, std::vector<Person>>(),
        "system vec map");

void test_class() {
  static uint64_t id = 0;

  if (!g_person->GetListener(id)) {
    id = g_person->AddListener(
        [](const Person &old_value, const Person &new_value) {
          LOG_INFO(g_logger)
              << "g_person value change, old value:" << old_value.ToString()
              << ", new value:" << new_value.ToString();
        });
  }

  LOG_INFO(g_logger) << g_person->GetValue().ToString();

  for (const auto &i : g_person_map->GetValue()) {
    LOG_INFO(g_logger) << i.first << ":" << i.second.ToString();
  }

  for (const auto &i : g_person_vec_map->GetValue()) {
    LOG_INFO(g_logger) << i.first;
    for (const auto &j : i.second) {
      LOG_INFO(g_logger) << j.ToString();
    }
  }
}

////////////////////////////////////////////////////////////

template <class T>
std::string formatArray(const T &v) {
  std::stringstream ss;
  ss << "[";
  for (const auto &i : v) {
    ss << " " << i;
  }
  ss << " ]";
  return ss.str();
}

template <class T>
std::string formatMap(const T &m) {
  std::stringstream ss;
  ss << "{";
  for (const auto &i : m) {
    ss << " {" << i.first << ":" << i.second << "}";
  }
  ss << " }";
  return ss.str();
}

void test_config() {
  LOG_INFO(g_logger) << "g_int value: " << g_int->GetValue();
  LOG_INFO(g_logger) << "g_float value: " << g_float->GetValue();
  LOG_INFO(g_logger) << "g_string value: " << g_string->GetValue();
  LOG_INFO(g_logger) << "g_int_vec value: "
                     << formatArray<std::vector<int>>(g_int_vec->GetValue());
  LOG_INFO(g_logger) << "g_int_list value: "
                     << formatArray<std::list<int>>(g_int_list->GetValue());
  LOG_INFO(g_logger) << "g_int_set value: "
                     << formatArray<std::set<int>>(g_int_set->GetValue());
  LOG_INFO(g_logger) << "g_int_unordered_set value: "
                     << formatArray<std::unordered_set<int>>(
                            g_int_unordered_set->GetValue());
  LOG_INFO(g_logger) << "g_int_map value: "
                     << formatMap<std::map<std::string, int>>(
                            g_map_string2int->GetValue());
  LOG_INFO(g_logger) << "g_int_unordered_map value: "
                     << formatMap<std::unordered_map<std::string, int>>(
                            g_unordered_map_string2int->GetValue());

  // 自定义配置项
  test_class();
}

int main(int argc, char *argv[]) {
  // 设置g_int的配置变更回调函数
  g_int->AddListener([](const int &old_value, const int &new_value) {
    LOG_INFO(g_logger) << "g_int value changed, old_value: " << old_value
                       << ", new_value: " << new_value;
  });

  LOG_INFO(g_logger) << "before============================";

  test_config();

  // 从配置文件中加载配置，由于更新了配置，会触发配置项的配置变更回调函数
  serverframework::EnvMgr::GetInstance()->Init(argc, argv);
  serverframework::Config::LoadFromConfDir("conf");
  LOG_INFO(g_logger) << "after============================";

  test_config();

  // 遍历所有配置
  serverframework::Config::Visit([](serverframework::ConfigVarBase::ptr var) {
    LOG_INFO(g_logger) << "name=" << var->GetName()
                       << " description=" << var->GetDescription()
                       << " typename=" << var->GetTypeName()
                       << " value=" << var->ToString();
  });

  return 0;
}
