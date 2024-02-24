#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"

namespace sylar {

class IOManager : public Scheduler {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event { // 事件类型
        NONE = 0x0,
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

        // 获得事件
        EventContext& getContext(Event event);
        void resetContext(EventContext& ctx);
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
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAll(int fd);

    // 获取当前的IO调度器
    static IOManager* GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;

    void contextResize(size_t size);

private:
    int m_epfd = 0;
    int m_tickleFds[2]; // pipe
    std::atomic<size_t> m_pendingEventCount = {0};
    RWMutexType m_mutex;
    std::vector<FdContext*> m_fdContexts;
};

}

#endif