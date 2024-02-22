#include "scheduler.h"
#include "log.h"
#include "macro.h"


namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


// 协程调度器
static thread_local Scheduler* t_scheduler = nullptr;
// 线程主协程
static thread_local Fiber* t_fiber = nullptr;


Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : m_name(name) {

    // 确定线程数量大于0 
    SYLAR_ASSERT(threads > 0);
    // 是否将当前用于协程调度线程也纳入调度器
    if(use_caller) {
        sylar::Fiber::GetThis();    // 这里获得的主协程用于调度其余协程
        -- threads; // 线程数-1
        
        SYLAR_ASSERT(GetThis() == nullptr); // 防止出现多个调度器
        // 设置当前的协程调度器
        t_scheduler = this;

        // 将此fiber设置为 use_caller，协程则会与 Fiber::MainFunc() 绑定
        // 非静态成员函数需要传递this指针作为第一个参数，用 std::bind()进行绑定
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this))); // 这个新协程用于执行方法
        // 设置线程名称
        sylar::Thread::SetName(m_name);
        // 设置当前线程的主协程为m_rootFiber
        // 这里的m_rootFiber是该线程的主协程（执行run任务的协程），只有默认构造出来的fiber才是主协程
        t_fiber = m_rootFiber.get();
        // 获得当前线程id
        m_rootThread = sylar::GetTreadId();
        m_threadIds.push_back(m_rootThread);
    } else { // 不将当前线程纳入调度器
        m_rootThread = -1;
    }
    // 更新线程数量
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    // 达到停止条件
    SYLAR_ASSERT(m_stopping);
    if(GetThis() == this) {
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}
Fiber* Scheduler::GetMainFiber() {
    return t_fiber;
}

void Scheduler::start() {
    SYLAR_LOG_INFO(g_logger) << "start()";
    MutexType::Lock lock(m_mutex);
    // 为false代表已经启动了，直接返回
    if(!m_stopping) {
        return;
    }
    // 将停止状态更新为false
    m_stopping = false;
    // 线程池为空
    SYLAR_ASSERT(m_threads.empty());
    // 创建线程池
    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++ i) {
        // 遍历每一个线程执行run任务
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + " " + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }

    lock.unlock();
    
    // 在这里切换线程时，swap的话会将线程的主协程与当前协程交换，
    // 当使用use_caller时，t_fiber = m_rootFiber，call是将当前协程与主协程交换
    // 为了确保在启动之后仍有任务加入任务队列中，
    // 所以在stop()中做该线程的启动，这样就不会漏掉任务队列中的任务
    if(m_rootFiber) {
        // m_rootFiber->swapIn();
        m_rootFiber->call();
        SYLAR_LOG_INFO(g_logger) << "call out" << m_rootFiber->getState();
    }
    SYLAR_LOG_INFO(g_logger) << "start() end";
}
void Scheduler::stop() {
    SYLAR_LOG_INFO(g_logger) << "stop()";
    m_autoStop = true;
    // 使用use_caller,并且只有一个线程，并且主协程的状态为结束或者初始化
    if(m_rootFiber && m_threadCount == 0 && (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::INIT)) {
        SYLAR_LOG_INFO(g_logger) << this << " stopped";
        // 停止状态为true
        m_stopping = true;
        // 若达到停止条件则直接return
        if(stopping()) {
            return;
        }
    }
    // bool exit_on_this_fiber = false;

    // use_caller线程
    // 当前调度器和t_secheduler相同
    if(m_rootThread != -1) {
        SYLAR_ASSERT(GetThis() == this);
    } else {
        SYLAR_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    // 每个线程都tickle一下
    for(size_t i = 0; i < m_threadCount; ++ i) {
        tickle();
    }
    // 使用use_caller多tickle一下
    if(m_rootFiber) {
        tickle();
    }

    if(stopping()) {
        return;
    }
}



void Scheduler::setThis() {
    t_scheduler = this;
}

void Scheduler::run() {
    SYLAR_LOG_INFO(g_logger) << "run()";
    // 设置当前调度器
    setThis();
    // 非user_caller线程，设置主协程为线程主协程
    if(sylar::GetTreadId() != m_rootThread) {
        t_fiber = Fiber::GetThis().get();
    }
    // 定义idle_fiber，当任务队列中的任务执行完之后，执行idle()
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    // 定义回调协程
    Fiber::ptr cb_fiber;
    // 定义一个任务结构体
    FiberAndThread ft;
    while(true) {
        // 重置也是一个初始化
        ft.reset();
        bool tickle_me = false;
        {
            // 从任务队列中拿fiber和cb
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();
            while(it != m_fibers.end()) {
                // 如果当前任务指定的线程不是当前线程，则跳过，并且tickle一下
                if(it->thread != -1 && it->thread != sylar::GetTreadId()) {
                    ++ it;
                    tickle_me = true;
                    continue;
                }
                // 确保fiber或cb存在
                SYLAR_ASSERT(it->fiber || it->cb);
                // 如果该fiber正在执行则跳过
                if(it->fiber && it->fiber->getState() == Fiber::EXEC) {
                    ++ it;
                    continue;
                }
                // 取出该任务
                ft = *it;
                // 从任务队列中清除
                m_fibers.erase(it);
            }
        }
        // 取到任务tickle一下
        if(tickle_me) {
            tickle();
        }

        // 执行拿到的线程
        if(ft.fiber && (ft.fiber->getState() != Fiber::TERM || ft.fiber->getState() != Fiber::EXCEPT)) {
            ++ m_activeThreadCount;
            // 执行任务
            ft.fiber->swapIn();
            // 执行完成，活跃的线程数量减-1
            -- m_activeThreadCount;

            ft.fiber->swapIn();
            // 如果线程的状态被置为了READY
            if(ft.fiber->getState() == Fiber::READY) {
                // 将fiber重新加入到任务队列中
                schedule(ft.fiber);
            } else if(ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {
                ft.fiber->m_state = Fiber::HOLD;
            }
            // 执行完毕重置数据ft
            ft.reset();
        // 如果任务是回调函数
        } else if(ft.cb) {
            // cb_fiber存在，重置该fiber
            if(cb_fiber) {
                cb_fiber->reset(ft.cb);
            } else {
                 // cb_fiber不存在则初始化一个
                cb_fiber.reset(new Fiber(ft.cb));
                ft.cb = nullptr;
            }
            // 重置数据ft
            ft.reset();
            ++ m_activeThreadCount;
            // 执行cb任务
            cb_fiber->swapIn();
            -- m_activeThreadCount;
            // 若cb_fiber状态为READY
            if(cb_fiber->getState() == Fiber::READY) {
                // 重新放入任务队列中
                schedule(cb_fiber);
                // 释放智能指针
                cb_fiber.reset();
            // cb_fiber异常或结束，就重置状态，可以再次使用该cb_fiber
            } else if(cb_fiber->getState() == Fiber::EXCEPT || cb_fiber->getState() == Fiber::TERM) {
                // cb_fiber的执行任务置空
                cb_fiber->reset(nullptr);
            } else {
                // 设置状态为HOLD，此任务后面还会通过ft.fiber被拉起
                cb_fiber->m_state = Fiber::HOLD;
                // 释放该智能指针，调用下一个任务时要重新new一个新的cb_fiber
                cb_fiber.reset();
            }
        // 没有任务执行
        } else {
            // 如果idle_fiber的状态为TERM则结束循环，真正的结束
            if(idle_fiber->getState() == Fiber::TERM) {
                SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                break;
            }
            // 正在执行idle的线程数量+1
            ++ m_idleThreadCount;
            // 执行idle()
            // 正在执行idle的线程数量-1
            idle_fiber->swapIn();
            -- m_idleThreadCount;
            // idle_fiber状态置为HOLD
            if(idle_fiber->getState() != Fiber::TERM
                    && idle_fiber->getState() != Fiber::EXCEPT) {
                idle_fiber->m_state = Fiber::HOLD;
            }
            
        }
    }
}

void Scheduler::tickle() {
    SYLAR_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    // 当自动停止 && 正在停止 && 任务队列为空 && 活跃的线程数量为0
    return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    SYLAR_LOG_INFO(g_logger) << "idle";
}

}