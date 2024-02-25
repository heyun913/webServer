#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include <memory>
#include <vector>
#include <list>
#include <iostream>
#include<atomic>
#include "fiber.h"
#include "thread.h"

// 协程调度器， 封装的是N-M的协程调度器，内部有一个线程池,支持协程在线程池里面切换
namespace sylar {

class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    // 线程数量；在运用协程调度的同时，为true则也要进行线程调度，线程池的名称
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    virtual ~Scheduler();
    // 返回协程调度器名称
    const std::string& getName() const { return m_name; }
    // 返回当前的协程调度器
    static Scheduler* GetThis();
    // 返回当前协程调度器的调度协程
    static Fiber* GetMainFiber();
    // 启动协程调度器
    void start();
    // 停止协程调度器
    void stop();

    // 调度协程模板函数
    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1) { // -1表示任意线程
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }
       if(need_tickle) {
            tickle();
       }
    }

    // 批量处理调度协程
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end, int thread = -1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end) {
                need_tickle = scheduleNoLock(&*begin, thread) || need_tickle;
                ++ begin;
            }
        }
       if(need_tickle) {
            tickle();
        }
    }
protected:
    // 通知协程调度器有任务了
    virtual void tickle();
    void run(); // 真正执行协程调度的方法
    // 返回是否可以停止
    virtual bool stopping();
    // 协程无任务可调度时执行idle协程
    virtual void idle();
    // 设置当前的协程调度器
    void setThis();
    // 是否有空闲线程
    bool hasIdleThreads() { return m_idleThreadCount > 0; }
private:
    // 协程调度启动(无锁)
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        bool need_tickle = m_fibers.empty();
        FiberAndThread ft(fc, thread);
        if(ft.fiber || ft.cb) {
            m_fibers.push_back(ft);
        }
        return need_tickle;
    }

private:
    // 协程/函数/线程组
    struct FiberAndThread {
        Fiber::ptr fiber; // 协程
        std::function<void()> cb; // 回调函数
        int thread; // 线程ID

        FiberAndThread(Fiber::ptr f, int thr)
            : fiber(f), thread(thr) {}

        FiberAndThread(Fiber::ptr* f, int thr)
            :thread(thr) {
            // 因为传入的是一个智能指针，我们使用后会造成引用数加一，可能会引发释放问题，这里swap相当于把传入的智能指针变成一个空指针
            // 这样原智能指针的计数就会保持不变
            fiber.swap(*f); 
        }

        FiberAndThread(std::function<void()> f, int thr)
            : cb(f), thread(thr) {}  
        
        FiberAndThread(std::function<void()>* f, int thr)
            :thread(thr) {
                cb.swap(*f);
        }  

        // 将一个类用到STL中必须要有默认构造函数，否则无法进行初始化
        FiberAndThread()
            : thread(-1) {

        }

        // 重置
        void reset() {
            fiber = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

private:
    MutexType m_mutex;  // 互斥量
    std::vector<Thread::ptr> m_threads; // 线程池
    std::list<FiberAndThread> m_fibers;   // 等待执行的协程队列
    Fiber::ptr m_rootFiber;  // 主协程
    std::string m_name; // 协程调度器名称

protected:
    std::vector<int> m_threadIds; // 协程下的线程id数组
    size_t m_threadCount = 0;   // 线程数量
    std::atomic<size_t> m_activeThreadCount = {0}; // 工作线程数量
    std::atomic<size_t> m_idleThreadCount = {0}; // 空闲线程数量
    bool m_stopping = true; // 是否正在停止
    bool m_autoStop = false; // m_autoStop
    int m_rootThread = 0; // 主线程id(use_caller)
};


}

#endif