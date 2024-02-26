#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace sylar {

class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;
    // 事件类型
    enum Event { 
        NONE = 0x0, // 无事件
        READ = 0x1, // EPOLLIN
        WRITE = 0x4 // EPOLLOUT
    };
private:
    // 句柄上下文
    struct FdContext {
        typedef Mutex MutexType;
        // 事件类
        struct EventContext {
            Scheduler* scheduler = nullptr; // 事件执行的调度器
            Fiber::ptr fiber; // 事件协程
            std::function<void()> cb; // 事件的回调函数
        };

        // 获得事件上下文
        EventContext& getContext(Event event);
        // 重置事件上下文
        void resetContext(EventContext& ctx);
        // 触发事件
        void triggerEvent(Event event);

        int fd = 0; // 事件关联的句柄
        EventContext read; // 读事件
        EventContext write; // 写事件
        Event events = NONE; // 已注册的事件
        MutexType mutex;
    };
public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    ~IOManager();

    // 0 success, -1 error
    // 添加事件
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    // 删除事件
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAll(int fd);

    // 获取当前的IO调度器
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    bool stopping(uint64_t& timeout);
    void idle() override;
    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);

private:
    int m_epfd = 0; // epoll文件句柄
    int m_tickleFds[2]; // pipe文件句柄，其中fd[0]表示读端，fd[1] 表示写端
    std::atomic<size_t> m_pendingEventCount = {0}; // 等待执行的事件数量
    RWMutexType m_mutex; // 互斥锁
    std::vector<FdContext*> m_fdContexts; //socket事件上下文容器
};

}

#endif