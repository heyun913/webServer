#include"log.h"
#include<iostream>
#include<map>
#include<functional>
#include<time.h>
#include<string.h>
#include"config.h"

namespace sylar {

const char* LogLevel::ToString(LogLevel::Level level) {
    switch(level) { // [宏函数的使用]
#define XX(name) \
        case LogLevel::name: \
            return #name; \
            break;
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKONW";
    }
    return "UNKONW";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level,v) \
    if(str == #v) { \
        return LogLevel::level; \
    }
    XX(DEBUG,debug);
    XX(INFO,info);
    XX(WARN,warn);
    XX(ERROR,error);
    XX(FATAL,fatal);

    XX(DEBUG,DEBUG);
    XX(INFO,INFO);
    XX(WARN,WARN);
    XX(ERROR,ERROR);
    XX(FATAL,FATAL);
    return LogLevel::UNKNOW;
#undef XX
}

LogEventWrap::LogEventWrap(LogEvent::ptr e)
    : m_event(e)  {

}

LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event); // 把自己写入日志
}

std::stringstream& LogEventWrap::getSS() {
    return m_event->getSS();
}

void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);  //引入stdarg.h
    format(fmt, al);
    va_end(al);
}


void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1) {
        m_ss << std::string(buf, len);
        free(buf);
    }
}

class NkeyFormatItem : public LogFormatter::FormatItem {
public:
    NkeyFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << "\n";
    }
};

class TabFormatItem : public LogFormatter::FormatItem { // 输出tab
public:
    TabFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << "\t";
    }
};

class FileNameFormatItem : public LogFormatter::FormatItem { 
public:
    FileNameFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getFile();
    }
};


class MessageFormatItem : public LogFormatter::FormatItem { 
public:
    MessageFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getcContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem { 
public:
    LevelFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

class LineFormatItem : public LogFormatter::FormatItem { 
public:
    LineFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class StringFormatItem : public LogFormatter::FormatItem { 
public:
    StringFormatItem(const std::string& str) 
        :m_string(str) {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << m_string;
    }
private:
    std::string m_string;
};

class EplaseFormatItem : public LogFormatter::FormatItem { 
public:
    EplaseFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getEplase();
    }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem { 
public:
    LoggerNameFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getLogger()->getName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem { 
public:
    ThreadIdFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

// 线程名称
class ThreadNameFormatItem : public LogFormatter::FormatItem { 
public:
    ThreadNameFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getThreadName();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem { 
public:
    FiberIdFormatItem(const std::string& fmt = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem { 
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S") 
        : m_format(format) {
            if(m_format.empty()) {
                m_format = "%Y-%m-%d %H:%M:%S";
            }
    }
    void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        // os << event->getTime();
        struct tm tm;
        time_t time = event->getTime(); // 当前时间戳 引入time.h
        localtime_r(&time, &tm); // 将时间戳转换为本地时间，并将结果存放在tm中
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger,LogLevel::Level level,const char* file, int32_t line, uint32_t eplase
            , uint32_t threadId, uint32_t fiberId, uint64_t time, const std::string& thread_name) 
            :m_file(file),
            m_line(line),
            m_eplase(eplase),
            m_threadId(threadId),
            m_fiberId(fiberId),
            m_time(time),
            m_logger(logger),
            m_level(level),
            m_threadName(thread_name) {

}


Logger::Logger(const std::string& name) 
        :m_name(name),
        m_level(LogLevel::DEBUG) {
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n")); // 时间 线程号 携程号 日志级别 文件名 文件路径:行号 消息
}

void Logger::setFormatter(LogFormatter::ptr val) {
    // 加锁
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    // 防止所有的appender在任何情况下都直接继承log的格式
    for(auto& i : m_appender) { // 遍历该log的appender
        MutexType::Lock ll(i->m_mutex);
        if(!i->m_hasFormatter) { // 如果当前appender没有自己的格式，才则继承log的格式
            i->m_formatter = m_formatter;
        }
    }
}
void Logger::setFormatter(const std::string& val) {
    // 把字符串解析成格式
    sylar::LogFormatter::ptr new_val(new sylar::LogFormatter(val));
    if(new_val->isError()) {
        std::cout << "Logger setFormatter name = " << m_name
                << " value = " << val << " invalid formatter"
                << std:: endl;
        return;
    }
    setFormatter(new_val);
}

void LogAppender::setFormatter(LogFormatter::ptr val) {
    // 加写锁
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    if(m_formatter) {
        m_hasFormatter = true;
    } else {
        m_hasFormatter = false;
    }
}

LogFormatter::ptr LogAppender::getFormatter() {
    // 加读锁
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

LogFormatter::ptr Logger::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    if(!appender->getFormatter()) {
        MutexType::Lock ll(appender->m_mutex);
        appender->m_formatter = m_formatter; // 没有设置hasFormatter
    }
    m_appender.push_back(appender);
}           
void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appender.begin(); it != m_appender.end(); ++ it) {
        if(*it == appender) {
            m_appender.erase(it);
            break;
        }
    }
}

 void Logger::clrAppender() {
    MutexType::Lock lock(m_mutex);
    m_appender.clear();
 }

void Logger::log(LogLevel::Level level, LogEvent::ptr event)  {
    if(level >= m_level) {
        auto self = shared_from_this();
        MutexType::Lock lock(m_mutex);
        if(!m_appender.empty()) {
            for(auto &i : m_appender) {
                i->log(self,level, event);
            } 
        } else if(m_root){
            m_root->log(level,event);
        }
        
    }
}

void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::DEBUG, event); 
}
void Logger::info(LogEvent::ptr event) {
    log(LogLevel::INFO, event); 
}
void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::WARN, event); 
}
void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::ERROR, event); 
}
void Logger::error(LogEvent::ptr event) {
    log(LogLevel::FATAL, event); 
}


FileLogAppender::FileLogAppender(const std::string& filename) 
    : m_filename(filename) {
    reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger,LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        uint64_t now = time(0);
        if(now != m_lastTime) {
            reopen();
            m_lastTime = now;
        }
        MutexType::Lock lock(m_mutex);
        if(!(m_filestream << m_formatter->format(logger,level,event))) {
            std::cout << "file error!" << std::endl;
        }
    }
}

std::string FileLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if(m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

bool FileLogAppender::reopen() {
    MutexType::Lock lock(m_mutex);
    if(m_filestream) {
        m_filestream.close();
    }

    m_filestream.open(m_filename,std::ios::app); // 追加写入文件
    return !!m_filestream; // [?]
}

void StdoutAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        MutexType::Lock lock(m_mutex);
        std::cout << m_formatter->format(logger,level,event) << std::endl; // namespace "std" has no member "cout" 加入iostream
    }
}

std::string StdoutAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutAppender";
    if(m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern) 
    : m_pattern(pattern) {
        init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) {
    std::stringstream ss;
    for(auto &i : m_items) {
        i->format(ss,logger,level,event);
      
    }
    return ss.str();
}

// 日志格式定义
void LogFormatter::init() {
    std::vector<std::tuple<std::string, std::string,int>> vec;  // [tuple]  str,format, type
    std::string nstr; //当前str
    for(size_t i = 0; i < m_pattern.size(); ++ i) {
        if(m_pattern[i] != '%') {
            nstr.append(1,m_pattern[i]); // [append]
            continue;
        }

        if((i + 1) < m_pattern.size()) {
            if(m_pattern[i + 1] == '%') {
                nstr.append(1, '%');
                continue;
            }
        }

        size_t n = i + 1;
        int fmt_status = 0;
        std::string str;
        std::string fmt;
        size_t fmt_begin = 0; // 
        while(n < m_pattern.size()) {
            if(!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}') { // 判断当前字符是否维空格
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if(fmt_status == 0) {
                if(m_pattern[n] == '{') {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // 解析格式
                    fmt_begin = n;
                    ++ n;
                    continue;
                }
            }else if(fmt_status == 1) {
                if(m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    // std::cout << fmt << std::endl;
                    fmt_status = 0;
                    ++ n;
                    break;
                }
            }
            ++ n;
            if (n == m_pattern.size()) {	//最后一个字符, 每次获得str都是走到下一个字符然后进行截取，所以只有最后一个字符需要特殊处理
				if (str.empty()) {
					str = m_pattern.substr(i + 1);
				}
			}
        }
        if(fmt_status == 0) {
            if(!nstr.empty()) {
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if(fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(std::make_tuple("<parse_error>", fmt, 0));
        } 
        // else if(fmt_status == 2) {
        //     if(!nstr.empty()) {
        //         vec.push_back(std::make_tuple(nstr, std::string(), 0));
        //         nstr.clear();
        //     }
        //     vec.push_back(std::make_tuple(str, fmt, 1));
        //     i = n;
        // }
            
    }
    if(!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, std::string(), 0));
    }
    // [function] 引入function
    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
        // {"m",[](const std::string& fmt) { return FormatItem::ptr(new MessageFormatItem(fmt)); } }
#define XX(str,C) \
        {#str, [] (const std::string& fmt) { return FormatItem::ptr(new C(fmt)); }}

        XX(m, MessageFormatItem),           //m:消息
        XX(p, LevelFormatItem),             //p:日志级别
        XX(r, EplaseFormatItem),            //r:累计毫秒数
        XX(c, LoggerNameFormatItem),        //c:日志名称
        XX(t, ThreadIdFormatItem),          //t:线程id
        XX(N, ThreadNameFormatItem),        //N:线程名称
        XX(n, NkeyFormatItem),              //n:换行
        XX(d, DateTimeFormatItem),          //d:时间
        XX(l, LineFormatItem),              //l:行号
        XX(F, FiberIdFormatItem),           //F:协程id
        XX(T, TabFormatItem),               //T:table
        XX(f, FileNameFormatItem)           //f:文件名
#undef XX
    };
    for(auto& i : vec) {
        if(std::get<2>(i) == 0) {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        } else {
            auto it = s_format_items.find(std::get<0>(i));
            if(it == s_format_items.end()) {
                // 检测到日志出问题
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            } else {
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }
        // std::cout << '(' << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ')' << std::endl;
    }
}

LoggerManager::LoggerManager() {
    m_root.reset(new Logger);
    m_root->addAppender(LogAppender::ptr(new StdoutAppender));
    m_loggers[m_root->m_name] =  m_root;
    init();
}

std::string Logger::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if(m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_formatter) {
         node["formatter"] = m_formatter->getPattern();
    }
    for(auto& i : m_appender) {
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }
    // 不存在则创建
    Logger::ptr logger(new Logger(name));
    // 创建的logger什么都没有，则根据root赋值
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

struct LogAppenderDefine {
    int type = 0;   // 1 File. 2 Stdout
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine& oth) const {
        return type == oth.type
            && level == oth.level
            && formatter == oth.formatter
            && file == oth.file;
    }

};

struct LogDefine{
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine& oth) const{
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == oth.appenders;
    }

    bool operator<(const LogDefine& oth) const {
        return name < oth.name;
    }
};

// check Here

template<>
class LexicalCast<std::string, LogDefine> {
public:
    LogDefine operator()(const std::string& v) {
        YAML::Node n = YAML::Load(v);
        LogDefine ld;
        if(!n["name"].IsDefined()) {
            std::cout << "log config error: name is null, " << n
                      << std::endl;
            throw std::logic_error("log config name is null");
        }
        ld.name = n["name"].as<std::string>();
        ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
        if(n["formatter"].IsDefined()) {
            ld.formatter = n["formatter"].as<std::string>();
        }

        if(n["appenders"].IsDefined()) {
            //std::cout << "==" << ld.name << " = " << n["appenders"].size() << std::endl;
            for(size_t x = 0; x < n["appenders"].size(); ++x) {
                auto a = n["appenders"][x];
                if(!a["type"].IsDefined()) {
                    std::cout << "log config error: appender type is null, " << a
                              << std::endl;
                    continue;
                }
                std::string type = a["type"].as<std::string>();
                LogAppenderDefine lad;
                if(type == "FileLogAppender") {
                    lad.type = 1;
                    if(!a["file"].IsDefined()) {
                        std::cout << "log config error: fileappender file is null, " << a
                              << std::endl;
                        continue;
                    }
                    lad.file = a["file"].as<std::string>();
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else if(type == "StdoutLogAppender") {
                    lad.type = 2;
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else {
                    std::cout << "log config error: appender type is invalid, " << a
                              << std::endl;
                    continue;
                }

                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator()(const LogDefine& i) {
        YAML::Node n;
        n["name"] = i.name;
        if(i.level != LogLevel::UNKNOW) {
            n["level"] = LogLevel::ToString(i.level);
        }
        if(i.formatter.empty()) {
            n["formatter"] = i.formatter;
        }

        for(auto& a : i.appenders) {
            YAML::Node na;
            if(a.type == 1) {
                na["type"] = "FileLogAppender";
                na["file"] = a.file;
            } else if(a.type == 2) {
                na["type"] = "StdoutLogAppender";
            }
            if(a.level != LogLevel::UNKNOW) {
                na["level"] = LogLevel::ToString(a.level);
            }

            if(!a.formatter.empty()) {
                na["formatter"] = a.formatter;
            }

            n["appenders"].push_back(na);
        }
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
};


sylar::ConfigVar<std::set<LogDefine> >::ptr g_log_defines =
    sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
    LogIniter() {
        // 给日志绑定一个事件
          g_log_defines->addListener([](const std::set<LogDefine>& old_value,
                    const std::set<LogDefine>& new_value){
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
            for(auto& i : new_value) {
                auto it = old_value.find(i);
                sylar::Logger::ptr logger;
                if(it == old_value.end()) {
                    //新增logger
                    logger = SYLAR_LOG_NAME(i.name);
                } else {
                    if(!(i == *it)) {
                        //修改的logger
                        logger = SYLAR_LOG_NAME(i.name);
                    } else {
                        continue;
                    }
                }
                logger->setLevel(i.level);
                if(!i.formatter.empty()) {
                    logger->setFormatter(i.formatter);
                }

                logger->clrAppender();
                for(auto& a : i.appenders) {
                    sylar::LogAppender::ptr ap;
                    if(a.type == 1) {
                        ap.reset(new FileLogAppender(a.file));
                    } else if(a.type == 2) {
                        ap.reset(new StdoutAppender);
                    }
                    ap->setLevel(a.level);
                    if(!a.formatter.empty()) {
                        LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                        if(!fmt->isError()) {
                            ap->setFormatter(fmt);
                        } else {
                            std::cout << "log.name=" << i.name << " appender type=" << a.type
                                      << " formatter=" << a.formatter << " is invalid" << std::endl;
                        }
                    }
                    logger->addAppender(ap);
                }
            }

            for(auto& i : old_value) {
                auto it = new_value.find(i);
                if(it == new_value.end()) {
                    //删除logger
                    auto logger = SYLAR_LOG_NAME(i.name);
                    logger->setLevel((LogLevel::Level)0);
                    logger->clrAppender();
                }
            }
        });
    }
};

static LogIniter __log_init; // main之前执行

std::string LoggerManager::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    for(auto& i : m_loggers) {
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void LoggerManager::init() {

}


}


