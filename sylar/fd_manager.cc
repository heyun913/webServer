#include"fd_manager.h"
#include"hook.h"
#include <sys/stat.h>
#include <fcntl.h>
#include<vector>


namespace sylar {

// 构造函数
FdCtx::FdCtx(int fd)
    : m_isInit(false)
    , m_isSocket(false)
    , m_sysNonblock(false)
    , m_userNonblock(false)
    , m_isClosed(false)
    , m_fd(fd)
    , m_recvTimeout(-1)
    , m_sendTimeout(-1) {
    init();
}

FdCtx::~FdCtx() {
}


// 初始化
bool FdCtx::init() {
    // 如果已经初始化直接返回true
    if(m_isInit) {
        return true;
    }
    // 默认发送/接收超时时间
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    // 定义一个stat结构体
    struct stat fd_stat;
    // 通过文件描述符取得文件的状态,返回-1失败
    if(-1 == fstat(m_fd, &fd_stat)) {
        // 初始化失败并且不是Socket
        m_isInit = false;
        m_isSocket = false;
    } else {
        // 初始化成功
        m_isInit = true;
        // S_ISSOCK (st_mode) 是否为socket 
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    // 如果是socket，则给它设置为阻塞状态
    if(m_isSocket) {
        // 获取文件的flags
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        // 判断是否是阻塞的
        if(!(flags & O_NONBLOCK)) {
            // 不是则设置为阻塞
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        // 阻塞设置生效
        m_sysNonblock = true;
    } else {
        // 不是Socket则不管
        m_sysNonblock = false;
    }
    // 初始化用户没有设置为阻塞
    m_userNonblock = false;
    // 未关闭
    m_isClosed = false;
    // 反正初始化状态
    return m_isInit;
}

// 设置超时时间
void FdCtx::setTimeout(int type, uint64_t v) {
    // 套接字为设置Socket接收数据的超时时间
    if(type == SO_RCVTIMEO) {
        m_recvTimeout = v;
    } else {
        m_sendTimeout = v;
    }
}

// 获取超时时间
uint64_t FdCtx::getTimeout(int type) {
    if(type == SO_RCVTIMEO) {
        return m_recvTimeout;
    } else {
        return m_sendTimeout;
    }
}

// 构造函数
FdManager::FdManager() {
    m_datas.resize(64);
}

// 获取/创建文件句柄类
FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    RWMutexType::ReadLock lock(m_mutex);
    // 表示集合中没有，并且也不自动创建，直接返回空指针
    if((int)m_datas.size() <= fd) {
        if(auto_create ==false) {
            return nullptr;
        }
    } else {
        // 当前有值或者不需要创建，直接返回目标值
        if(m_datas[fd] || !auto_create) {
            return m_datas[fd];
        }
    }
    lock.unlock();
    // 自动创建
    RWMutexType::WriteLock lock2(m_mutex);
    FdCtx::ptr ctx(new FdCtx(fd));
    m_datas[fd] = ctx;
    return ctx;
}

// 删除文件句柄类
void FdManager::del(int fd) {
    RWMutexType::WriteLock lock(m_mutex);
    // 没找到直接返回
    if((int)m_datas.size() <= fd) {
        return;
    }
    // 删除
    m_datas[fd].reset();
}

}