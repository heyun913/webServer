#include"../sylar/config.h"
#include<iostream>

namespace sylar {


ConfigvarBase::ptr Config::LookupBase(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

static void ListAllMember(const std::string& prefix,
                            const YAML::Node& node,
                            std::list<std::pair<std::string, const YAML::Node>>& output) {

    // 判断prefix的合法性
    if (prefix.find_first_not_of("abcdefghigklmnopqrstuvwxyz._012345678") 
        != std::string::npos) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Config invalid name: " << prefix << " ! " << node;
        return;
    }


    output.push_back(std::make_pair(prefix, node));
    // 如果node是一个对象
    if(node.IsMap()) {
        for(auto it = node.begin(); it != node.end(); ++ it) {
            // 遍历对象里面的元素
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);
    // std::cout << "hhhyy" << all_nodes.size() << std::endl;
    // size_t cnt = 0;
    // for(auto& i : all_nodes) {
    //     std::cout << i.first << i.second << std::endl; 
    //     std::cout << cnt ++ << std::endl;

    // }
    // 检验ListAllMember的遍历结果是否正确

    for(auto& i : all_nodes) {
        std::string key = i.first; 
        // std::cout << "I.first: " << i.first << std::endl;
        //  std::cout << i.first << " : " << i.second << std::endl;
        if(key.empty()) continue;

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigvarBase::ptr var = LookupBase(key);

        // 如果找到
        if(var) {
            // std::cout << "var.first: " << i.first  << "var.second:  " << i.second << std::endl;
            if(i.second.IsScalar()) {
                var->fromString(i.second.Scalar());
            } else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}


void Config::Visit(std::function<void(ConfigvarBase::ptr)> cb) {
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = GetDatas();
    for(auto it = m.begin(); it != m.end(); ++ it) {
        cb(it->second);
    }
}

}