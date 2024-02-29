#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <mutex>
#include <atomic>
#include "noncopyable.h"

namespace sylar {


// 信号量
class Semaphore : Noncopyable {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void notify();
private:
    sem_t m_semaphpre;
};

// 局部锁
template<class T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T& mutex)
        : m_mutex(mutex) {
            m_mutex.lock();
            m_locked = true;
        }
    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 局部读锁模板
template<class T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex)
        : m_mutex(mutex) {
            m_mutex.rdlock();
            m_locked = true;
        }
    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 局部写锁模板
template<class T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex)
        : m_mutex(mutex) {
            m_mutex.wrlock();
            m_locked = true;
        }
    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 互斥量
class Mutex : Noncopyable {
public:
    typedef ScopedLockImpl<Mutex> Lock;

    Mutex() {
        pthread_mutex_init(&m_mutex,nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};


// 空的Mutex，测试线程安全
class NullMutex : Noncopyable{
public:
    typedef ScopedLockImpl<NullMutex> Lock;
    NullMutex() {}
    ~NullMutex() {}
    void lock() {}
    void unlock() {}
};

// 空的读写锁
class NullRWMutex : Noncopyable {
public:
    typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
    typedef WriteScopedLockImpl<NullRWMutex> WriteLock;
    NullRWMutex() {
    }
    ~NullRWMutex() {
    }
    // 读
    void rdlock() {
    }
    // 写
    void wrlock() {
    }
    // 解锁
    void unlock() {
    }
};

// 读写锁
class RWMutex : Noncopyable{
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;
    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }
    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }
    // 读
    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }
    // 写
    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }
    // 解锁
    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

// 优化锁的性能:自旋锁
class Spinlock : Noncopyable {
public:
    typedef ScopedLockImpl<Spinlock> Lock;
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }
    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    void lock() {
        pthread_spin_lock(&m_mutex);
    }
    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }
private:
    pthread_spinlock_t m_mutex;

};

// 优化锁：CAS
class CASLock : Noncopyable{ 
public:
    CASLock() {
        m_mutex.clear();
    }

    ~CASLock() {

    }
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex,std::memory_order_acquire));
    }

    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    volatile std::atomic_flag m_mutex;
};


class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    // 获取线程ID
    pid_t getId() const { return m_id; }
    // 获取线程名称
    const std::string& getName() const { return m_name; }
    // 等待线程执行完成
    void join();
    // 获取当前的线程指针
    static Thread* GetThis();
    // 获取当前的线程名称
    static const std::string& GetName();
    // 设置线程名称
    static void SetName(const std::string& name);
private:
    Thread(const Thread&) = delete;
    Thread(const Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;
    // 线程执行函数
    static void* run(void* arg);
private:
    pid_t m_id = -1;                     // 线程ID
    pthread_t m_thread = 0;             // 线程结构
    std::function<void()> m_cb;     // 线程执行函数
    std::string m_name;             // 线程名称
    Semaphore m_semaphore; // 信号量
};


}


#endif