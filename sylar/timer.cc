#include"timer.h"
#include"util.h"

namespace sylar {


// 比较函数，默认小于
bool Timer::Comparator::operator() (const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    // 两个都为空返回false
    if(!lhs && !lhs) return false;
    // 左边为空
    if(!lhs) return true;
    // 右边为空
    if(!rhs) return false;
    // 如果左边的执行时间 < 右边的执行时间
    if(lhs->m_next < rhs->m_next) return true;
    
    if(rhs->m_ms < lhs->m_ms) return false;
    // 如果时间都一样，则比较地址的大小
    return lhs.get() < rhs.get();
}

// Timer构造函数
Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager) 
    : m_recurring(recurring)
    , m_ms(ms)
    , m_cb(cb)
    , m_manager(manager) {
    m_next = sylar::GetCurrentMS() + m_ms;
}

Timer::Timer(uint64_t next) 
    : m_next(next) {

}

bool Timer::cancel() {
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    // 如果有回调函数
    if(m_cb) {
        // 置空
        m_cb = nullptr;
        // 从定时器管理器中找到需要取消的定时器
        auto it = m_manager->m_timers.find(shared_from_this());
        // 删除
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    // 如果没有需要执行的回调函数，直接返回刷新失败
    if(!m_cb) {
       return false;
    }
    // 找到需要刷新的定时器
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()) {
        return false;
    }
    // 一定要先删除再刷新，因为set会自动排序，先刷新后之前获得的迭代器就失效了
    m_manager->m_timers.erase(it);
    m_next = sylar::GetCurrentMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    // 若周期相同并且不按当前时间计算
    if(ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::RWMutextType::WriteLock lock(m_manager->m_mutex);
    // 如果没有需要执行的回调函数，直接返回重置失败
    if(!m_cb) {
       return false;
    }
    // 找到需要重置的定时器
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()) {
        return false;
    }
    // 还是先删除
    m_manager->m_timers.erase(it);
    // 定义一个起始时间
    uint64_t start = 0;
    // 从现在开始算
    if(from_now) {
        start = sylar::GetCurrentMS();
    } else {
        // 起始时间为当时创建时的起始时间
        //  m_next = sylar::GetCurrentMS() + m_ms;
        start = m_next - m_ms;
    }
    // 更新
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}

TimerManager::TimerManager() {
    m_previouseTime = sylar::GetCurrentMS();
}
TimerManager::~TimerManager() {

}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    // 创建定时器
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutextType::WriteLock lock(m_mutex);
    // 添加到定时器集合中
    addTimer(timer,lock);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    // 使用weak_cond的lock函数获取一个shared_ptr指针tmp
    std::shared_ptr<void> tmp = weak_cond.lock();
    // 如果tmp不为空，则调用回调函数cb。
    if(tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring) {
     // 在定时器触发时会调用 OnTimer 函数，并在OnTimer函数中判断条件对象是否存在，如果存在则调用回调函数cb。
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}


uint64_t TimerManager::getNextTimer() {
    RWMutextType::ReadLock lock(m_mutex);
    // 不触发 onTimerInsertedAtFront
    m_tickled = false;
    // 如果没有定时器要执行，返回一个最大数
    if(m_timers.empty()) {
        return ~0ull;
    }
    // 拿到第一个定时器
    const Timer::ptr& next = *m_timers.begin();
    uint64_t now_ms = sylar::GetCurrentMS();
    // 如果当前的时间超过了定时器执行的时间，说明由于某种原因耽搁了定时器的执行，直接返回0立即执行
    if(now_ms >= next->m_next) {
        return 0;
    // 否则返回下一个定时器需要等待的时间
    } else {
        return next->m_next - now_ms;
    }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
    // 获得当前的时间用于对比
    uint64_t now_ms = sylar::GetCurrentMS();
    // 定义需要执行的定时器数组
    std::vector<Timer::ptr> expired;
    {
        RWMutextType::ReadLock lock(m_mutex);
        // 如果定时器为空则直接返回
        if(m_timers.empty()) {
            return;
        }
    }
    RWMutextType::WriteLock lock(m_mutex);
    // 判断服务器时间是否被调整
    bool rollover = detectClockRollover(now_ms);
    // 如果服务器时间没问题，并且第一个定时器都没有到执行时间，就说明没有任务需要执行
    if(!rollover && ((*m_timers.begin())->m_next > now_ms)) {
        return;
    }
    // 记录当前的时间
    Timer::ptr now_timer(new Timer(now_ms));
    // 找到定时器容器中第一个小于当前时间的值
    auto it = rollover ? m_timers.end() :  m_timers.lower_bound(now_timer);
    // 这一步是为了把定时器容器中与当前时间相同的定时器包含进来
    while(it != m_timers.end() && (*it)->m_next == now_ms) {
        ++ it;
    }
    // 往超时容器中填充
    expired.insert(expired.begin(), m_timers.begin(), it);
    // 删除原来定时器容器中的已超时的定时器
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());
    // 将expired的timer放入到cbs中，这样在其它函数中调用listExpiredCb 就可以去执行这些回调函数
    for(auto& timer : expired) {
        cbs.push_back(timer->m_cb);
        // 如果是循环定时器，则再次放入定时器集合中
        if(timer->m_recurring) {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        } else {
            timer->m_cb = nullptr;
        }

    }
}

void TimerManager::addTimer(Timer::ptr val,RWMutextType::WriteLock& lock) {
    // 添加到定时器集合
    auto it = m_timers.insert(val).first;
    // 如果该定时器是超时时间最短 并且 没有设置触发onTimerInsertedAtFront
    // bool at_front = (it == m_timers.begin() && !m_tickled);
    bool at_front = (it == m_timers.begin()) && !m_tickled;
    if(at_front) {
        // 设置触发onTimerInsertedAtFront
        m_tickled = true; // 频繁修改时不用一直执行onTimerInsertedAtFront，提高效率 
    }
    lock.unlock();
    // 触发onTimerInsertedAtFront
    if(at_front) {
        // 目前该函数只做了一次tickle
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    // 如果当前时间比上次执行时间还小 并且 小于一个小时的时间，相当于时间倒流了
    if(now_ms < m_previouseTime && now_ms < (m_previouseTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    // 更新上次执行时间
    m_previouseTime = now_ms;
    return rollover;
}

bool TimerManager::hasTimer() {
    RWMutextType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

}