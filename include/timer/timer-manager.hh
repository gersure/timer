#pragma once

#include <unistd.h>
#include <sys/syscall.h>
#include <atomic>
#include <unordered_map>
#include <sys/timerfd.h>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>

#include "posix.hh"
#include "singleton.hh"
#include "timer.hh"
#include "timer-set.hh"
#include "thread_pool.hh"


class timer_manager : public Singleton<timer_manager>{
public:
    typedef  timer<> mtimer_t;
    typedef  mtimer_t::timer_id timer_id;
    typedef  mtimer_t::clock clock;
    typedef  mtimer_t::duration duration;
    typedef  mtimer_t::time_point time_point;
    typedef  mtimer_t::callback_t  callback_t;
    typedef  timer_set<mtimer_t>::timer_index  timer_index;
    //typedef  typename std::pair<timer_id, timer_index> timer_ret;
    using timer_ret = std::pair<timer_id, timer_index>;
private:
    int _timerfd = {};
    timer_set<mtimer_t> _timers;
    boost::shared_mutex   _timer_mutex;
    boost::shared_mutex   _fd_mutex;

    timer_manager();
    ~timer_manager();

public:
    std::shared_ptr<thread_pool> _thread_pool;
    void set_thread_pool(std::shared_ptr<thread_pool> pool);

    void complete_timers();
    void enable_timer(steady_clock_type::time_point when);
public:
    timer_ret add_timer(duration delta, callback_t&& callback);
    timer_ret add_timer(time_point when, duration delta, callback_t&& callback);
    void del_timer(timer_ret& ret);

    friend class timer<>;
    friend class Singleton<timer_manager>;
};

void timer_manager::enable_timer(steady_clock_type::time_point when)
{
    itimerspec its, old;
    its.it_interval = {};
    its.it_value = to_timespec(when);
    std::cout<<"enable_time:\t"<<its.it_value.tv_sec<<"s:"<<its.it_value.tv_nsec<<"ns"<<std::endl;
    auto ret = timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &its, &old);
    throw_system_error_on(ret == -1);
}

timer_manager::timer_ret  timer_manager::add_timer(duration delta, callback_t&& callback)
{
    std::pair<bool, timer_index> ret;
    mtimer_t t(delta, std::move(callback));

    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    ret = _timers.insert(t);

    if (ret.first)
        enable_timer(_timers.get_next_timeout());
    return {t.get_id(), ret.second};
}

timer_manager::timer_ret timer_manager::add_timer(time_point when, duration delta, callback_t&& callback)
{
    mtimer_t t;
    std::pair<bool, timer_index> ret;
    t.set_callback(std::move(callback));
    t.arm(when, delta);

    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    ret = _timers.insert(t);

    if (ret.first)
        enable_timer(_timers.get_next_timeout());
    return {t.get_id(), ret.second};
}

void timer_manager::del_timer(timer_ret& ret)
{
    {
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    _timers.remove(ret);
    }
}

timer_manager::timer_manager()
{
    _timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    throw_system_error_on(_timerfd == (-1), strerror(errno));
}


void timer_manager::complete_timers() {
    uint64_t  expire = {};
    auto ret = read(_timerfd, &expire, sizeof(expire));
//    if (ret != sizeof(uint64_t)){
//        return;
//    }
    std::cout<<"ret:"<<ret<<"expire:"<<expire<<std::endl;
    std::cout<<"-------------size:"<<_timers.size()<<std::endl;

    typename timer_set<mtimer_t>::timer_list_t  expired_timers;
    {
        boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
        expired_timers = _timers.expire(_timers.now());
    }
    for (auto& t : expired_timers) {
        t.second._expired = true;
    }
    for (auto& t : expired_timers) {
        if (t.second._armed) {
            t.second._armed = false;
            if (t.second._period) {
                t.second.arm(clock::now() + t.second._period.get(), {t.second._period.get()});
                _timers.insert(t.second);
            }
            try {
                _thread_pool->enqueue(t.second.get_callback());
            } catch (...) {
                std::runtime_error("Timer callback failed: {}");
            }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    boost::shared_lock<boost::shared_mutex> lk(_fd_mutex);
    if (!_timers.empty() && _timerfd > 0){
        enable_timer(_timers.get_next_timeout());
    }
    return;
}


timer_manager::~timer_manager()
{
    {
        boost::unique_lock<boost::shared_mutex> lk(_fd_mutex);
        close(_timerfd);
        _timerfd = 0;
    }
}

void timer_manager::set_thread_pool(std::shared_ptr<thread_pool> pool)
{
    _thread_pool.swap(pool);
}

void thread_pool::recycle()
{
    while (!stop.load(std::memory_order_relaxed))
    {
        timer_manager::Instance().complete_timers();
    }
}

