#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main(int argc, char** argv) {
    // logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutAppender)); 
    
    // sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    // sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%m%n"));
    // file_appender->setFormatter(fmt);
    // file_appender->setLevel(sylar::LogLevel::FATAL); // 过滤特定级别level
    // logger->addAppender(file_appender);


    // SYLAR_LOG_DEBUG(logger) << "asdasda";
    // SYLAR_LOG_FMT_FATAL(logger, "fmt eeror %s", "hy");

    // auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
    // SYLAR_LOG_INFO(l) << "XX";
    // std::cout << "asdasd" << std::endl;
    SYLAR_LOG_INFO(g_logger) << "asdasd";
    return 0;
}
