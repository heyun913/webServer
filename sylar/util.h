#ifndef __SYKAR_UTIL_H__
#define __SYKAR_UTIL_H__
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace sylar {

// 获取线程ID
pid_t GetTreadId();

// 获取协程ID
uint32_t GetFiberId();

// 获取函数调用栈
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1); // size：输出栈的大小；skip：跳过前几个输出

std::string BacktraceToString(int size = 64, int skip = 2,const std::string& prefix = "");

// 时间ms
uint64_t GetCurrentMS(); // 毫秒
uint64_t GetCurrentUS(); // 微秒

}


#endif