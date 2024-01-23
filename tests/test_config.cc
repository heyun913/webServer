#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"
#include"../sylar/config.h"
#include"yaml-cpp/yaml.h"

sylar::ConfigVar<int>::ptr g_int_value_config = 
    sylar::Config::Lookup("system.port", (int)8080, "system port");

sylar::ConfigVar<float>::ptr g_int_valuex_config = 
    sylar::Config::Lookup("system.port", (float)8080, "system port");

sylar::ConfigVar<float>::ptr g_float_value_config = 
    sylar::Config::Lookup("system.value", (float)10.2f, "system value");


sylar::ConfigVar<std::vector<int>>::ptr g_vec_value_config = 
    sylar::Config::Lookup("system.vec", std::vector<int>{1,2}, "system vec");

sylar::ConfigVar<std::list<int>>::ptr g_list_value_config = 
    sylar::Config::Lookup("system.list", std::list<int>{1,2}, "system list");

sylar::ConfigVar<std::set<int>>::ptr g_set_value_config = 
    sylar::Config::Lookup("system.set", std::set<int>{1,2}, "system set");

sylar::ConfigVar<std::unordered_set<int>>::ptr g_uset_value_config = 
    sylar::Config::Lookup("system.uset", std::unordered_set<int>{1,2}, "system uset");

sylar::ConfigVar<std::map<std::string, int>>::ptr g_str_int_map_value_config = 
    sylar::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k",2}}, "system str_int_map");

sylar::ConfigVar<std::unordered_map<std::string, int>>::ptr g_str_int_umap_value_config = 
    sylar::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k",2}}, "system str_int_umap");

void print_yaml(const YAML::Node& node, int level) {
    if(node.IsScalar()) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') 
            << node.Scalar() << " - " << node.Type() << " - " << level;
    } else if(node.IsNull()) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')  << "NULL - " << node.Type() << " - " << level;
    } else if(node.IsMap()) {
        for(auto it = node.begin(); it != node.end(); ++ it) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')
                << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    } else if(node.IsSequence()) {
        for(size_t i = 0; i < node.size(); ++ i) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')
                << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}



void test_yaml() {
    YAML::Node root = YAML::LoadFile("/root/Web-learning/sylar/bin/conf/log.yml");
    print_yaml(root, 0);
}

void test_config() {
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before:" << g_int_value_config->getValue(); 
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before:" << g_float_value_config->toString();

#define XX(g_var,name,prefix) \
    { \
        auto v = g_var->getValue(); \
        for(auto& i : v) { \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name ": " << i; \
        } \
    }

#define XX_M(g_var,name,prefix) \
    { \
        auto v = g_var->getValue(); \
        for(auto& i : v) { \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name ": {" << i.first << " : " << i.second << "}"; \
        } \
    }

    XX(g_vec_value_config, vec, before);
    XX(g_list_value_config, list, before);
    XX(g_set_value_config, set, before);
    XX(g_uset_value_config, set, before);
    XX_M(g_str_int_map_value_config, smap, before);
    XX_M(g_str_int_umap_value_config, smap, before);

    YAML::Node root = YAML::LoadFile("/root/Web-learning/sylar/bin/conf/log.yml");
    sylar::Config::LoadFromYaml(root);
    
    XX(g_vec_value_config, vec, after);
    XX(g_list_value_config, list, after);
    XX(g_set_value_config, set, after);
    XX(g_uset_value_config, uset, after);
    XX_M(g_str_int_map_value_config, smap, after);
    XX_M(g_str_int_umap_value_config, umap, after);
}

class Person {
public:
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name = " << m_name
           << " age = " << m_age
           << " sex = " << m_sex
           << "]";
        return ss.str();
    }

    bool operator==(const Person& oth) const{
        return m_name == oth.m_name && m_age == oth.m_age && m_sex == oth.m_sex;
    }
};
// 1.自定义类型
sylar::ConfigVar<Person>::ptr g_person = 
    sylar::Config::Lookup("class.person", Person(), "system person");
// 2.自定义类型与STL嵌套
sylar::ConfigVar<std::map<std::string, Person>>::ptr g_person_map = 
    sylar::Config::Lookup("class.map", std::map<std::string, Person>(), "system map");
// 3.更加复杂的类型map<string,vector<Person>>
sylar::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map = 
    sylar::Config::Lookup("class.vec_map", std::map<std::string,  std::vector<Person>>(), "system vec_map");

namespace sylar {
// 复杂类型转换 偏特化[string -> Person]
template<>
class LexicalCast<std::string, Person> {
public:
    Person operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};

// 复杂类型转换 偏特化[Person -> string]
template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator()(const Person& p) {
        // 定义一个node容器
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
}

void test_class() { // 自定义类型测试
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "1.before: " << g_person->getValue().toString() << " - " << g_person->toString();
#define XX_PM(g_var,name,prefix) \
    { \
        auto v = g_var->getValue(); \
        for(auto& i : v) { \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name ": " << i.first << " - " << i.second.toString(); \
        } \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " size = " << v.size(); \
    }
    
    // 监控配置的使用
    g_person->addListener([](const Person& old_vale, const Person& new_vale){
         SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "oldVale: " << old_vale.toString()
            << " newValue: " << new_vale.toString();
    });

    XX_PM(g_person_map, person_map, 2.before);
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "3.before" << g_person_vec_map->getValue().size();
    YAML::Node root = YAML::LoadFile("/root/Web-learning/sylar/bin/conf/log.yml");
    sylar::Config::LoadFromYaml(root);
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "1.after: " << g_person->getValue().toString() << " - " << g_person->toString();
    XX_PM(g_person_map, person_map, 2.after);
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "3.after" << g_person_vec_map->toString();
}


void test_log() {
    static sylar::Logger::ptr system_log = SYLAR_LOG_NAME("system");
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/root/Web-learning/sylar/bin/conf/log.yml");
    sylar::Config::LoadFromYaml(root);
    std::cout << "======================" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << root << std::endl;
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;
    //
    system_log->setFormatter("%d - %m%n");
    SYLAR_LOG_INFO(system_log) << "hello system" << std::endl;
}

int main(int argc, char** argv) {
    test_log();
    sylar::Config::Visit([](sylar::ConfigvarBase::ptr var) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "name = " << var->getName()
                        << " description = " << var->getDescription()
                        << " typename = " << var->getTypeName()
                        << " value = " << var->toString();
    });
    return 0;
}