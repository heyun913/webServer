#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

#include<unistd.h>

namespace sylar {
    // 当前线程是否hook
    bool is_hook_enable();
    // 设置当前线程的hook状态
    void set_hook_enable(bool flag);
}

// 应用c风格
extern "C" {

// 重新定义同名的接口

// sleep_fun 为函数指针
typedef unsigned int (*sleep_fun)(unsigned int seconds);
// 它是一个sleep_fun类型的函数指针变量，表示该变量在其他文件中已经定义，我们只是在当前文件中引用它。
extern sleep_fun sleep_f;

typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;

}

#endif