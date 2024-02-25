#include"iomanager.h"
#include"macro.h"
#include"log.h"

#include<sys/epoll.h>
#include<unistd.h>
#include<fcntl.h>
#include<error.h>
#include<string.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event) {
    switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "getContext");
    }
}

void IOManager::FdContext::resetContext(EventContext& ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    // 保证注册事件中有需要触发的事件
    SYLAR_ASSERT(events & event);
    // 触发了一个事件就将其从注册事件中删除
    events = (Event)(events & ~event);
    // 获取触发事件的上下文
    EventContext& ctx = getContext(event);
    // 根据传入的是线程或者回调函数执行调度
    if(ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    // 执行完毕将协程调度器置空
    ctx.scheduler = nullptr;
    return;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name) 
    : Scheduler(threads, use_caller, name) {
    // 创建一个epollfd
    m_epfd = epoll_create(5000);
    // 断言是否成功
    SYLAR_ASSERT(m_epfd > 0);
    // 创建管道用于进程通信
    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(!rt);
    // 创建事件并初始化
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    // 注册读事件，设置边缘触发模式
    event.events = EPOLLIN | EPOLLET;
    // 将fd关联到pipe的读端
    event.data.fd = m_tickleFds[0];
    // 对一个打开的文件描述符执行一系列控制操作
    // F_SETFL: 获取/设置文件状态标志
    // O_NONBLOCK: 使I/O变成非阻塞模式，在读取不到数据或是写入缓冲区已满会马上return，而不会阻塞等待。
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK); // fcntl可以改变句柄的属性
    SYLAR_ASSERT(!rt);
    // 将pipe的读端注册到epoll
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    SYLAR_ASSERT(!rt);
    // 初始化socket事件上下文vector
    contextResize(32);
    // 启动调度器
    start();
}
IOManager::~IOManager() {
    // 停止调度器
    stop();
    // 释放epoll
    close(m_epfd);
    // 关闭pipe
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);
    // 释放m_fdContexts内存
    for(size_t i = 0; i < m_fdContexts.size(); ++ i) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++ i) {
        // 没有则创建
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}
int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    // 初始化一个 FdContext
    FdContext* fd_ctx = nullptr;
    // 加读锁
    RWMutexType::ReadLock lock(m_mutex);
    // 先判断句柄是否超出范围
    if((int)m_fdContexts.size() > fd) {
        // 空间足够
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else {
        // 空间不够就扩容1.5倍
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }   

    // 设置fd上下文的状态
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    // 一个句柄一般不会重复加同一个事件， 可能是两个不同的线程在操控同一个句柄添加事件
    if(fd_ctx->events & event) {
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd
            << "event = " << event
            << " fd_ctx.event = " << fd_ctx->events;
        SYLAR_ASSERT(!(fd_ctx->events & event));
    }
    // 若已经有注册的事件则为修改操作，若没有则为添加操作
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    // 创建一个epoll事件
    epoll_event epevent;
    // 设置边缘触发模式，添加原有的事件以及要注册的事件
    epevent.events = EPOLLET | fd_ctx->events | event;
    // 将fd_ctx存到data的指针中
    epevent.data.ptr = fd_ctx;
    // 注册事件
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ","  << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return -1;
    }
    // 等待执行的事件数量+1
    ++ m_pendingEventCount;
    // 将 fd_ctx 的注册事件更新
    fd_ctx->events = (Event)(fd_ctx->events | event);
    // 获得对应事件的 EventContext
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    // EventContext的成员应该都为空
    SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.cb && !event_ctx.fiber);
    // 对event_ctx的三个成员赋值
    event_ctx.scheduler = Scheduler::GetThis();
    // 如果有回调就执行回调，没有就执行该协程
    if(cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();
        SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    return 0;
}
bool IOManager::delEvent(int fd, Event event) {
    // 加读锁
    RWMutexType::ReadLock lock(m_mutex);
    // 如果存放fd上下文的容器数量大小小于当前要删除事件的fd，说明出现问题
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    // 找到要删除fd对应的FdContext
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    // 若没有要删除的事件
    if(!(fd_ctx->events & event)) {
        return false;
    }
    // 将事件从注册事件中删除
    Event new_events = (Event)(fd_ctx->events & ~event);
    // 若还有事件则是修改，若没事件了则删除
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    // 水平触发模式，新的注册事件
    epevent.events = EPOLLET | new_events;
    // ptr 关联 fd_ctx
    epevent.data.ptr = fd_ctx;
    // 注册事件
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ","  << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }
    // 等待执行的事件数量-1
    -- m_pendingEventCount;
    // 更新事件
    fd_ctx->events = new_events;
    // 拿到对应事件的EventContext
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    // 重置EventContext
    fd_ctx->resetContext(event_ctx);
    return true;
}
bool IOManager::cancelEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)) {
        return false;
    }
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ","  << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    fd_ctx->triggerEvent(event);
    -- m_pendingEventCount;
   
    return true;
}
bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(fd_ctx->events) {
        return false;
    }
    // 删除操作
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    // 定义为没有任何事件
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << op << ","  << fd << "," << epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }
    // 有读事件执行读事件
    if(fd_ctx->events & READ) {
        fd_ctx->triggerEvent(READ);
        -- m_pendingEventCount;
    }
    // 有写事件执行写事件
    if(fd_ctx->events & WRITE) {
        fd_ctx->triggerEvent(WRITE);
        -- m_pendingEventCount;
    }
    // 最后确保事件为空
    SYLAR_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    // 有消息时写入一个数据提示
    if(!hasIdleThreads()) { // 如果没有空闲线程
        return;
    }
    // 有任务来了，就往 pipe 里发送1个字节的数据，这样 epoll_wait 就会唤醒
    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1);
}
bool IOManager::stopping(uint64_t& timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}
void IOManager::idle() {
    epoll_event* events = new epoll_event[64]();
    // 使用智能指针托管events， 离开idle自动释放
    std::shared_ptr<epoll_event> shared_events(events,[](epoll_event* ptr) {
        delete[] ptr;
    });

    while(true) {
        // 下一个任务要执行的时间
        uint64_t next_timeout = 0;
        // 获得下一个执行任务的时间，并且判断是否达到停止条件
        if(stopping(next_timeout)) {
            SYLAR_LOG_INFO(g_logger) << "name = " << getName() << " idle stopping exit";
            break;
        }

        int rt = 0;
        do {
            // 最大定时器睡眠时长
            static const int MAX_TIMEOUT = 3000;
            // 如果有定时器任务
            if(next_timeout != ~0ull) {
                // 睡眠时间不能超过MAX_TIMEOUT
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            } else {
                // 没有任务就睡眠MAX_TIMEOUT
                next_timeout = MAX_TIMEOUT;
            }
             /*  
             * 阻塞在这里，但有3种情况能够唤醒epoll_wait
             * 1. 超时时间到了
             * 2. 关注的 soket 有数据来了
             * 3. 通过 tickle 往 pipe 里发数据，表明有任务来了
             */
            rt = epoll_wait(m_epfd, events, 64, (int)next_timeout);
            // 操作系统中断会返回EINTR，然后重新epoll_wait
            if(rt < 0 && errno == EINTR) {

            } else {
                break;
            }
        } while(true);

        // 找到那些需要执行的定时器，这里调用listExpiredCb返回的应该是那些超时的定时器，难道是超时代表需要在当前时间去处理吗？我之前理解的是超时就丢弃了
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()) {
            // 把那些超时任务全部放到队列中去
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }
        // 遍历准备好了的fd
        for(int i = 0; i < rt; ++ i) {
            // 从 events 中拿一个 event
            epoll_event& event = events[i];
            // 如果获得的这个信息时来自 pipe
            if(event.data.fd == m_tickleFds[0]) {
                uint8_t dummy;
                // 将 pipe 发来的1个字节数据读掉
                while(read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }
            // 从 ptr 中拿出 FdContext
            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            // 在源码中，注册事件时内核会自动关注POLLERR和POLLHUP
            if(event.events & (EPOLLERR | EPOLLHUP)) {
                // 将读写事件都加上
                event.events |= EPOLLIN | EPOLLOUT;
                // event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            int real_events = NONE;
            // 读事件
            if(event.events & EPOLLIN) {
                real_events |= READ;
            }
            // 写事件
            if(event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            // 没有时间
            if((fd_ctx->events & real_events) == NONE) {
                continue;
            }
            // 获得剩余的事件
            int left_events = (fd_ctx->events & ~real_events);
            // 如果执行完该事件还有事件则修改，若无事件则删除
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            // 更新新的事件
            event.events = EPOLLET | left_events;

            // 重新注册事件
            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2) {
                SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << op << ","  << fd_ctx->fd << "," << event.events << "):"
                << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }
            // 读事件好了，执行读事件
            if(real_events & READ) {
                fd_ctx->triggerEvent(READ);
                -- m_pendingEventCount;
            }
            // 写事件好了，执行写事件
            if(real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                -- m_pendingEventCount;
            }
        }
        // 执行完epoll_wait返回的事件
        // 获得当前协程
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();
        // 执行完返回scheduler的MainFiber 继续下一轮
        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

}