#ifndef __SYKAR_FIBER_H__
#define __SYKAR_FIBER_H__

#include <ucontext.h>
#include <memory>
#include "thread.h"
#include <functional>

namespace sylar {

// 继承了enable_shared_from_this不能在栈上创建成员
class Scheduler;
class Fiber : public std::enable_shared_from_this<Fiber> {
friend class Scheduler;
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State { // 
        // 初始化状态
        INIT,
        // 暂停状态
        HOLD,
        // 执行中状态
        EXEC,
        // 结束状态
        TERM,
        // 可执行状态
        READY,
        // 异常状态
        EXCEPT
    };

private:
    Fiber();
public:
    Fiber(std::function<void()> cb, size_t stacksize = 0);
    ~Fiber();
    // 重置协程函数，并重置状态
    void reset(std::function<void()> cb);
    // 切换到当前协程执行
    void swapIn();
    // 切换到后台执行
    void swapOut();
    
    void call();

    uint64_t getId() const { return m_id; }

    State getState() const { return m_state; }
public:
    // 设置当前协程
    static void SetThis(Fiber* f);
    // 返回当前协程
    static Fiber::ptr GetThis();
    // 协程切换到后台，并且设置为Ready状态
    static void YieldToReady();
    //协程切换到后台，并且设置为Hold状态
    static void YieldToHold();
    // 总协程数
    static uint64_t TotalFibers();
    // 返回协程ID
    static uint64_t GetFiberId();

    static void MainFunc();
private:
    uint64_t m_id = 0;  //协程id
    uint32_t m_stacksize = 0;   // 携程运行栈的大小
    State m_state = INIT;   //协程状态
    ucontext_t m_ctx;   //上下文
    void* m_stack = nullptr;    //协程运行栈指针

    std::function<void()> m_cb; // 协程执行方法
};

}


#endif