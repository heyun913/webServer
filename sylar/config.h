#ifndef __SYLAY_CONFIG_H__
#define __SYLAY_CONFIG_H__
#include<memory>
#include<sstream>
#include<iostream>
#include<string.h>
#include<boost/lexical_cast.hpp>  //[类型转换]
#include"../sylar/log.h"
#include"yaml-cpp/yaml.h"
#include<vector>
#include<list>
#include<map>
#include<set>
#include<unordered_map>
#include<unordered_set>
#include<functional>  // [推荐了解]
#include "thread.h"
#include "log.h"

namespace sylar {

// 配置基类 放置一些公用的属性
class ConfigvarBase {
public:
    typedef std::shared_ptr<ConfigvarBase> ptr;
    ConfigvarBase(const std::string name, const std::string description = "")
        :m_name(name)
        ,m_description(description) {
            // 统一将字母转化为小写
            std::transform(m_name.begin(), m_name.end(), m_name.begin(),::tolower);
        }                                                               // 构造函数
    virtual ~ConfigvarBase() {}                                         // 析构函数

    const std::string& getName() const { return m_name; }               // 返回配置参数名称
    const std::string& getDescription() const { return m_description; } // 返回配置参数描述
    virtual std::string toString() = 0;                                 // 转换成字符串
    virtual bool fromString(const std::string& val) = 0;                // 从字符串初始化值
    virtual std::string getTypeName() const = 0;
protected:
    std::string m_name;                                                 // 配置参数的名称
    std::string m_description;                                          // 配置参数的描述
};

// 基础类型转换 把F转换成T类型
template<class F, class T>
class LexicalCast {
public:
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};

// 复杂类型转换 偏特化[string -> vector]
template<class T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str())); // 这里相当于调用基础类型转换，如果传入的vec里面元素是int类型，那么就是string -> int
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[vector -> string]
template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 基础类型转换
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 复杂类型转换 偏特化[string -> List]
template<class T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        typename std::list<T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str())); // 这里相当于调用基础类型转换，如果传入的vec里面元素是int类型，那么就是string -> int
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[List -> string]
template<class T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 基础类型转换
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 复杂类型转换 偏特化[string -> Set]
template<class T>
class LexicalCast<std::string, std::set<T>> {
public:
    std::set<T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        typename std::set<T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str())); // 这里相当于调用基础类型转换，如果传入的vec里面元素是int类型，那么就是string -> int
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[Set -> string]
template<class T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 基础类型转换
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 复杂类型转换 偏特化[string -> uSet]
template<class T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        std::unordered_set<T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str())); // 这里相当于调用基础类型转换，如果传入的vec里面元素是int类型，那么就是string -> int
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[uSet -> string]
template<class T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 基础类型转换
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


// 复杂类型转换 偏特化[string -> map]
template<class T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    std::map<std::string, T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        std::map<std::string,T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(auto it = node.begin(); it != node.end(); ++ it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str()))); 
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[map -> string]
template<class T>
class LexicalCast<std::map<std::string,T>, std::string> {
public:
    std::string operator()(const std::map<std::string,T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


// 复杂类型转换 偏特化[string -> umap]
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    std::unordered_map<std::string, T> operator()(const std::string& v) {
        // 调用YAML中的load函数，接收一个string类型输入，将其转换成node结构
        YAML::Node node = YAML::Load(v);
        std::unordered_map<std::string,T> vec;     // 定义结果vec
        std::stringstream ss;   // 使用字符串流来获取node中的每个string值
        for(auto it = node.begin(); it != node.end(); ++ it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str()))); 
        }
        return vec;
    }
};

// 复杂类型转换 偏特化[umap -> string]
template<class T>
class LexicalCast<std::unordered_map<std::string,T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string,T>& v) {
        // 定义一个node容器
        YAML::Node node;
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 配置参数类
template<class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigvarBase {
public:
    typedef RWMutex RWMutexType;    // 配置写少读多，所以用读写锁好一些
    typedef std::shared_ptr<ConfigVar> ptr;
    // 定义配置更改的回调函数，当一个值更改时可以知道原始值和新值
    typedef std::function<void (const T& old_value, const T& new_value)> on_change_cb;


    ConfigVar(const std::string& name, const T& default_value, const std::string& description = "") // 初始化配置名称、参数值以及参数描述
        :ConfigvarBase(name,description)
        ,m_val(default_value) {

        }
    std::string toString() override {   // 将参数值转换为string类型
        try {
            // return boost::lexical_cast<std::string>(m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        } catch(std::exception& e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                << e.what() << "convert: " << typeid(m_val).name() << " to string";
        }
        return "";
    }
    bool fromString(const std::string& val) override {  // 从string转换为参数值
        try {
            // m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));
        } catch (std::exception& e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception "
                << e.what() << " convert: string to " << typeid(m_val).name();
        }
        return false;
    }

    const T getValue() { 
        RWMutexType::ReadLock lock(m_mutex);
        return m_val; 
    }
    void setValue(const T& v) { 
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(v == m_val) {        // 如果新值和原值一样，说明无变化直接返回
                return;
            }
            for(auto& i : m_cbs) {  // 有变化则通过map中key找到对应配置更改
                i.second(m_val, v);
            }
        } // 增加一个局部作用域，释放掉上面的读锁
        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }
    std::string getTypeName() const override{ return typeid(T).name(); } // 返回当前值的数据类型


    //  添加监听函数
    u_int64_t addListener(on_change_cb cb) {
        // 因为functional是没有比较函数的，所以这里我们生成一个静态的唯一id，这样每次生成监听函数时，程序会自动分配一个ID
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        ++ s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }
    // 删除监听函数
    void delListener(uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }
    // 获取监听函数
    on_change_cb getListener(uint64_t key) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }
    // 清空所有监听器
    void clearListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }

private:
    RWMutexType m_mutex;
    T m_val;
    // 变更回调数组，[为什么使用map?] -> (因为functional没有比较函数，如果要删除一个function，或者在容器里面判断是否是一个相等的函数是无法做到的)
    // key值就可以辅助我们删除,要求唯一可以使用hash
    std::map<uint64_t,on_change_cb> m_cbs;
   
};


// 配置管理类
class Config {
public:
    typedef std::unordered_map<std::string, ConfigvarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;
    /**
     * @brief 获取/创建对应参数名的配置参数
     * @param[in] name 配置参数名称
     * @param[in] default_value 参数默认值
     * @param[in] description 参数描述
     * @details 获取参数名为name的配置参数,如果存在直接返回
     *          如果不存在,创建参数配置并用default_value赋值
     * @return 返回对应的配置参数,如果参数名存在但是类型不匹配则返回nullptr
     * @exception 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
     */
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = "") {
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it != GetDatas().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
            if(tmp) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name = " << name << " exists"; 
                return tmp;
            } else {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name = " << name 
                    << " exists but type not " << typeid(T).name()
                    << " realType= " << it->second->getTypeName(); 
                return nullptr;
            }
        }

        if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678")
                != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetDatas()[name] = v;
        return v;
    }

    /**
     * @brief 查找配置参数
     * @param[in] name 配置参数名称
     * @return 返回配置参数名为name的配置参数
     */
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it == GetDatas().end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
    }

    /**
     * @brief 使用YAML::Node初始化配置模块
     */
    static void LoadFromYaml(const YAML::Node& root);

    /**
     * @brief 加载path文件夹里面的配置文件
     */
    static void LoadFromConfDir(const std::string& path, bool force = false);

    /**
     * @brief 查找配置参数,返回配置参数的基类
     * @param[in] name 配置参数名称
     */
    static ConfigvarBase::ptr LookupBase(const std::string& name);

    // 返回配置map
    static void Visit(std::function<void(ConfigvarBase::ptr)> cb);

private:

    /**
     * @brief 返回所有的配置项
     */
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }
    static RWMutexType& GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }

};

}

#endif