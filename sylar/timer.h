#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include<memory>
#include<set>
#include<vector>
#include"thread.h"

namespace sylar {

class TimerManager;
class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    // 定时器的智能指针类型
    typedef std::shared_ptr<Timer> ptr;
    // 取消定时器
    bool cancel();
    // 刷新设置定时器的执行时间
    bool refresh();
    // 重置定时器时间
    bool reset(uint64_t ms, bool from_now);
private:
    // 构造函数
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);

    Timer(uint64_t next);
   
private:
    bool m_recurring = false; // 是否循环定时器
    uint64_t m_ms = 0; // 执行周期
    uint64_t m_next = 0; // 精确的执行时间
    std::function<void()> m_cb; // 回调函数
    TimerManager* m_manager = nullptr; // 定时器管理器

private:
    struct Comparator {
        bool operator() (const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

class TimerManager {
friend class Timer;
public:
    typedef RWMutex RWMutextType;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
    // 添加条件定时器
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

    // 下一个定时器的执行时间
    uint64_t getNextTimer();
    // 返回那些超时的定时器
    void listExpiredCb(std::vector<std::function<void()>>& cbs);

    bool hasTimer();
protected:
    virtual void onTimerInsertedAtFront() = 0;
    void addTimer(Timer::ptr val, RWMutextType::WriteLock& lock);
    
private:
    bool detectClockRollover(uint64_t now_ms);
private:
    RWMutextType m_mutex;
    std::set<Timer::ptr, Timer::Comparator> m_timers; // 定时器集合
    bool m_tickled = false; // 是否触发onTimerInsertedAtFront
    uint64_t m_previouseTime = 0; // 上一次执行时间
};


}


#endif