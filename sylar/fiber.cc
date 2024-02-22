#include "fiber.h"
#include "config.h"
#include "macro.h"
#include <atomic>
#include "log.h"
#include"scheduler.h"

namespace sylar {


static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static std::atomic<uint64_t> s_fiber_id {0};    // 生成协程ID
static std::atomic<uint64_t> s_fiber_count {0}; // 统计当前的协程数

static thread_local Fiber* t_fiber = nullptr;   // 当前协程
static thread_local Fiber::ptr t_threadFiber = nullptr;    // 主协程

// 设置协程栈的大小为1MB
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

// 创建/释放运行栈
class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }
    static void Dealloc(void* vp, size_t size) {
        return free(vp);
    }

};

// 设置别名
using StackAllocator = MallocStackAllocator;

// 主协程的构造
Fiber::Fiber() {
    m_state = EXEC;
    // 设置当前协程
    SetThis(this);
    // 获取当前协程的上下文信息保存到m_ctx中
    if(getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false,"getcontext");
    }

    ++ s_fiber_count;

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

// 子协程的构造
Fiber::Fiber(std::function<void()> cb, size_t stacksize)
    : m_id(++s_fiber_id)
    , m_cb(cb) {
    
    ++ s_fiber_count;
    // 若给了初始化值则用给定值，若没有则用约定值
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
    // 获得协程的运行指针
    m_stack = StackAllocator::Alloc(m_stacksize);
    // 保存当前协程上下文信息到m_ctx中
    if(getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false,"getcontext");
    }
    // uc_link为空，执行完当前context之后退出程序。
    m_ctx.uc_link= nullptr;
    // 初始化栈指针
    m_ctx.uc_stack.ss_sp = m_stack;
    // 初始化栈大小
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id = " << m_id;
}

// 释放协程运行栈
Fiber::~Fiber() {
    --s_fiber_count;
    // 子协程
    if(m_stack) {
        // 不在准备和运行状态
        SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
        // 释放运行栈
        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {
        // 主协程的释放要保证没有任务并且当前正在运行
        SYLAR_ASSERT(!m_cb);
        SYLAR_ASSERT(m_state == EXEC);
        // 若当前协程为主协程，将当前协程置为空
        Fiber* cur = t_fiber;
        if(cur == this) {
            SetThis(nullptr);
        }
    }
    SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber id = " << m_id;
}


// 重置协程
void Fiber::reset(std::function<void()> cb) {
    // 要求栈空间
    SYLAR_ASSERT(m_stack);
    // 要求状态只能为结束或者初始状态
    SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
    m_cb = cb;
    if(getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false,"getcontext");
    }
    // 重置
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}
// 设置当前协程
void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

void Fiber::call() { // 特殊的swapIn,强行将当前线程置换成目标线程
    SetThis(this);
    m_state = EXEC;
    // SYLAR_ASSERT(GetThis() == t_threadFiber);
    if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
        SYLAR_ASSERT2(false, "swapcontext");
    }
}

void Fiber::swapIn() {
    SetThis(this);
    SYLAR_ASSERT(m_state != EXEC);
    // 因为要执行切换，所以改为运行状态
    m_state =EXEC;
    if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
        SYLAR_ASSERT2(false, "swapcontext");
    }

}

void Fiber::swapOut() {
    if(this != Scheduler::GetMainFiber()) {
        SetThis(Scheduler::GetMainFiber());
        if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    } else {
        SetThis(t_threadFiber.get());
        if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }

}

Fiber::ptr Fiber::GetThis() {
    // 返回当前协程
    if(t_fiber) {
        return t_fiber->shared_from_this();
    }
    // 获得主协程
    Fiber::ptr main_fiber(new Fiber);
    // 确保当前协程为主协程
    SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}

void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    cur->m_state = READY;
    cur->swapOut();
}

void Fiber::YieldToHold() {
     Fiber::ptr cur = GetThis();
    cur->m_state = HOLD;
    cur->swapOut();
}

uint64_t TotalFibers() {
    return s_fiber_count;
}

uint64_t Fiber::GetFiberId() {
    if(t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    } catch(std::exception& ex) {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what() 
            << " fiber_id=" << cur->getId()
            << std::endl << sylar::BacktraceToString(); 
    } catch(...) {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " 
            << " fiber_id=" << cur->getId()
            << std::endl << sylar::BacktraceToString(); 
    }
    // 这里cur获得一个智能指针导致计数器加一，但是由于最后没有释放，它会一直在栈上面，所以
    // 该对象的智能指针计数永远大于等于1，无法被释放
    auto raw_ptr = cur.get();
    cur.reset();
    // 返回主协程
    raw_ptr->swapOut();

    SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
}


}