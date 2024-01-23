#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_semaphpre, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}
Semaphore::~Semaphore() {
    sem_destroy(&m_semaphpre);
}

void Semaphore::wait() {
    if(sem_wait(&m_semaphpre)) {
        throw std::logic_error("sem_wait error");
    }
}
void Semaphore::notify() {
    if(sem_post(&m_semaphpre)) {
        throw std::logic_error("sem_post error");
    }
}


// 定义线程局部变量
static thread_local Thread* t_thread = nullptr;
// 定义线程局部变量的名称
static thread_local std::string t_thread_name = "UNKNOW";
// 定义系统日志
static sylar::Logger::ptr g_looger = SYLAR_LOG_NAME("system");

// 获取当前的线程指针
Thread* Thread::GetThis() {
    return t_thread;
}
// 获取当前的线程名称
const std::string& Thread::GetName() {
    return t_thread_name;
}
// 设置线程名称
void Thread::SetName(const std::string& name) {
    if(t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}
// 构造函数
Thread::Thread(std::function<void()> cb, const std::string& name)
    :m_cb(cb),m_name(name){
    if(name.empty()) {
        m_name = "UNKNOW";
    }
    // 第一个参数为pthread_t类型，第二个参数一般为空，用于定制线程的不同属性，第三个参数为线程启动函数（注意类型需要是void*）,第四个参数表示线程启动函数的参数值
    // 没有时可以设置为null，有的话传入参数地址
    // 整个函数正常时返回0
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if(rt) {
        SYLAR_LOG_ERROR(g_looger) << "pthread_create thread fail, rt=" << rt << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    m_semaphore.wait();
}
// 析构函数
Thread::~Thread() {
    if(m_thread){
        pthread_detach(m_thread);
    }
}
// 等待线程执行完成
void Thread::join() {
    if(m_thread) {
        int rt = pthread_join(m_thread,nullptr);
        if(rt) {
             SYLAR_LOG_ERROR(g_looger) << "pthread_join thread fail, rt=" << rt << " m_name=" << m_name;
                throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}
// 线程执行函数
void* Thread::run(void* arg) {
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetTreadId();//获取线程ID
    // 该函数的作用是设置名称的名称，第一个参数：需要设置/获取 名称的线程；第二个参数：要设置/获取 名称的buffer（16个字符长）；
    pthread_setname_np(pthread_self(), thread->m_name.substr(0,15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb); // 在不涉及复制或移动可调用对象的情况下，快速地交换两个function的内容
    thread->m_semaphore.notify();
    cb();
    return 0;
}


}