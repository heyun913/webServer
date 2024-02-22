#include "../sylar/sylar.h"


sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "main";
    sylar::Scheduler sc;
    sc.start();
    sc.stop();
    SYLAR_LOG_INFO(g_logger) << "over";
    
    return 0;
}