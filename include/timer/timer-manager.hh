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
    typedef  timer mtimer_t;
    typedef  mtimer_t::clock clock;
    typedef  mtimer_t::duration duration;
    typedef  mtimer_t::time_point time_point;
    typedef  mtimer_t::callback_t  callback_t;
    //typedef  typename std::pair<timer_id, timer_index> timer_ret;
private:
    int _timerfd = {};
    timer_set _timers;
    boost::shared_mutex   _timer_mutex;
    boost::shared_mutex   _fd_mutex;

    timer_manager();
    ~timer_manager();

public:
    std::shared_ptr<thread_pool> _thread_pool;
    void set_thread_pool(std::shared_ptr<thread_pool> pool);

    static void completed(){
        timer_manager::Instance().enable_timer(timer_manager::clock::now());
        close(timer_manager::Instance()._timerfd);
    }
    void complete_timers();    
    void enable_timer(steady_clock_type::time_point when);
public:
    timer_handle add_timer(duration delta, callback_t&& callback);
    timer_handle add_timer(time_point when, duration delta, callback_t&& callback);
    void del_timer(timer_handle& ret);

    friend class timer;
    friend class timer_set;
    friend class timer_handle;
    friend class Singleton<timer_manager>;
};

void timer_manager::enable_timer(steady_clock_type::time_point when)
{
    itimerspec its, old;
    its.it_interval = {};
    its.it_value = to_timespec(when);
    auto ret = timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &its, &old);
    throw_system_error_on(ret == -1);
}

timer_handle  timer_manager::add_timer(duration delta, callback_t&& callback)
{
    mtimer_t *p = new mtimer_t(delta, std::move(callback));
    timer_handle h( p, p->get_id());
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    if (_timers.insert(h))
        enable_timer(_timers.get_next_timeout());
    return h;
}

timer_handle timer_manager::add_timer(time_point when, duration delta, callback_t&& callback)
{
    mtimer_t* p = new mtimer_t;
    p->set_callback(std::move(callback));
    p->arm(when, delta);
    timer_handle h(p, p->get_id());
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    if (_timers.insert(h))
        enable_timer(_timers.get_next_timeout());
    return h;
}

void timer_manager::del_timer(timer_handle& h)
{
    {
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    _timers.remove(h);
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
    typename timer_set::timer_set_t  expired_timers;
    {
        boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
        expired_timers = _timers.expire(_timers.now());
    }
    for (auto t : expired_timers) {
        t.get_timer()->_expired = true;
    }
    for (auto t : expired_timers) {
        if (t.get_timer()->_armed) {
            t.get_timer()->_armed = false;
            if (t.get_timer()->_period) {
                t.get_timer()->arm(clock::now() + t.get_timer()->_period.get(), {t.get_timer()->_period.get()});
                _timers.insert(t);
            }
            if (!_thread_pool->stopped())
                _thread_pool->enqueue(t.get_timer()->get_callback());
            else
                t.get_timer()->get_callback()();
            if (!t.get_timer()->_period){
                delete t.get_timer();
            }
        }else{
            delete t.get_timer();
        }
    }
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
        if (!thread_pool){
            close(_timerfd);
            _timerfd = 0;
        }
    }
}

void timer_manager::set_thread_pool(std::shared_ptr<thread_pool> pool)
{
    _thread_pool.swap(pool);
    _thread_pool->at_exit(timer_manager::completed);
}

void thread_pool::recycle()
{
    while (!stop.load(std::memory_order_relaxed))
    {
        timer_manager::Instance().complete_timers();
    }
}

