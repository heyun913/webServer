#include "util.h"
#include "log.h"
#include "fiber.h"
#include <execinfo.h>
#include<sys/time.h>
namespace sylar {

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

pid_t GetTreadId() { return syscall(SYS_gettid); }

uint32_t GetFiberId() {

    return sylar::Fiber::GetFiberId(); 
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    // 没有按照官方库的实现应用指针，这是因为指针会占用栈的空间，一般不应该往栈里面分配大的对象
    // 线程的栈一般较大，协程的栈可以我们自己设置，一般较小，便于轻量级切换
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = ::backtrace(array,size);

    char** strings = backtrace_symbols(array,s);
    if(strings == NULL) {
        SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error";
        return;
    }

    for(size_t i = skip; i < s; ++ i) {
        bt.push_back(strings[i]);
    }
    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
        std::vector<std::string> bt;
        Backtrace(bt,size,skip);
        std::stringstream ss;
        for(size_t i = 0; i < bt.size(); ++ i) {
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
}

uint64_t GetCurrentMS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}
uint64_t GetCurrentUS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

}