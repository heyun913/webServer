#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include<string>
#include<stdint.h>
#include<memory>
#include<list>
#include<vector>
#include<sstream>
#include<fstream>
#include<stdarg.h>
#include<map>
#include"singleton.h"
#include"util.h"
#include "thread.h"

// 流式输出
#define SYLAR_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger,level, \
            __FILE__,__LINE__,0,sylar::GetTreadId(), \
            sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getSS()


#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

// 格式化输出
#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, sylar::GetTreadId(),\
                sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

// 返回主日志器
#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

// 查找并返回一个日志
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar{        // 防止和别人的代码冲突

class Logger;           // <把Logger放到这里的目的?> 前面的一些类会用到Logger，不加会报未定义错误
class LoggerManager;    // 同上
// 自定义日志级别
class LogLevel {
public:
    enum Level{
        UNKNOW = 0,     //  未知 级别
        DEBUG = 1,      //  DEBUG 级别
        INFO = 2,       //  INFO 级别
        WARN = 3,       //  WARN 级别
        ERROR = 4,      //  ERROR 级别
        FATAL = 5       //  FATAL 级别
    };

    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};


// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr; // [智能指针]
    LogEvent(std::shared_ptr<Logger> logger,LogLevel::Level level,const char* file
            , int32_t line, uint32_t eplase
            , uint32_t threadId, uint32_t fiberId, uint64_t time
            , const std::string& thread_name);

    const char* getFile() const { return m_file;}
    int32_t getLine() const { return m_line; }
    uint32_t getEplase() const { return m_eplase; }
    uint32_t getThreadId() const { return m_threadId; }
    uint32_t getFiberId() const { return m_fiberId; }
    uint64_t getTime() const { return m_time; }
    const std::string getcContent() const { return m_ss.str(); }
    std::shared_ptr<Logger> getLogger() const { return m_logger; }
    LogLevel::Level getLevel() const { return m_level; }
    std::stringstream& getSS() { return m_ss; }
    const std::string& getThreadName() const { return m_threadName; }
    void format(const char* fmt, ...); 
    void format(const char* fmt, va_list al);
private:
    const char* m_file = nullptr;     // 文件名
    int32_t m_line = 0;               // 行号，引入头文件stdint.h
    uint32_t m_eplase = 0;            // 程序启动到现在的毫秒
    uint32_t m_threadId = 0;          // 线程ID
    uint32_t m_fiberId = 0;           // 协程ID
    uint64_t m_time;                  // 时间戳
    std::stringstream m_ss;           // 内容
    std::shared_ptr<Logger> m_logger; // 写入日志对象
    LogLevel::Level m_level;          // 日志级别
    std::string m_threadName;         // 线程名称
};

// 专门放置LogEvent
class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    std::stringstream& getSS();
    LogEvent::ptr getEvent() const { return m_event;}
private:
    LogEvent::ptr m_event;
};


// 日志格式
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);
    std::string format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);
public:
    class FormatItem {                          // [类中类]
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) = 0;
    };

    void init(); 

    bool isError() const { return m_error; }

    const std::string getPattern() const { return m_pattern; }
private:
    std::string m_pattern;                      // 解析格式
    std::vector<FormatItem::ptr> m_items;       // 解析内容
    bool m_error = false;                       // 格式错误标志
};

// 日志输出地
class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock MutexType;

    virtual ~LogAppender() {}                                           // 为了便于该类的派生类调用，定义为[虚类]，

    virtual void log(std::shared_ptr<Logger> logger,LogLevel::Level level, LogEvent::ptr event) = 0;   // [纯虚函数]，子类必须重写
    virtual std::string toYamlString() = 0;

    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getFormatter();

    void setLevel(LogLevel::Level level) { m_level = level; }
    LogLevel::Level getLevel() const { return m_level; }
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;                                            // 级别,为了便于子类访问该变量，设置在保护视图下
    LogFormatter::ptr m_formatter;                                      // 定义输出格式
    bool m_hasFormatter = false;                                        // 是否有自己的日志格式器
    MutexType m_mutex;                                                      // 锁
};

// 日志输出器
class Logger : public std::enable_shared_from_this<Logger>{ 
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock MutexType;
    
    Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    // 不同级别的日志输出函数
    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);
    void error(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);            // 添加一个appender
    void delAppender(LogAppender::ptr appender);            // 删除一个appender
    void clrAppender();                                     // 清空所有appender
    LogLevel::Level getLevel() const { return m_level; }    // [const放在函数后]
    void setLevel(LogLevel::Level val) { m_level = val; }   // 设置级别

    const std::string& getName() const { return m_name; }

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter();

    std::string toYamlString();

private:
    std::string m_name;                                     // 日志名称
    LogLevel::Level m_level;                                // 级别
    std::list<LogAppender::ptr> m_appender;                 // Appender集合,引入list
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;                                     // 如果一个logger没有appender之类的，可以通过该变量赋予root的appender
    MutexType m_mutex;                                      // 锁
};

// 输出方法分类

// 输出到控制台
class StdoutAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutAppender> ptr; 
    void log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
private:
};

// 输出到文件
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);                   // 输出的文件名
    void log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) override;  // [override]
    std::string toYamlString() override;

    bool reopen();                                                  // 重新打开文件，成功返回true
private:
    std::string m_filename;
    std::ofstream m_filestream;                                     // stringstream要报错，引入sstream
    uint64_t m_lastTime = 0;                                                
};


// 日志管理器
class LoggerManager {
public:
    typedef Spinlock MutexType;

    LoggerManager();
    // 调用时如果logger不存在，那么我们定义好再返回,之前是直接返回root
    Logger::ptr getLogger(const std::string& name);

    void init();
    Logger::ptr getRoot() const { return  m_root; }  // 获取主日志管理器

    std::string toYamlString();
private:
    MutexType m_mutex; // 锁
    std::map<std::string, Logger::ptr> m_loggers;    // 存储日志
    Logger::ptr m_root;                              // 默认日志
};

// 日志器管理类单例模型
typedef sylar::Singleton<LoggerManager> LoggerMgr;

} //   namespace end
#endif