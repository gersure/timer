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
#include "thread-pool.hh"
#include "timer-types.hh"

class timer_set {
private:
    static constexpr timestamp_t max_timestamp = std::numeric_limits<timestamp_t>::max();

    timer_set_t _buckets;
    timestamp_t _last;
    timestamp_t _next;
public:
    friend class timer;
    friend class timer_handle;

private:
    static timestamp_t get_timestamp(time_point _time_point){
        return _time_point.time_since_epoch().count();
    }
    static timestamp_t get_timestamp(timer& t){
        return get_timestamp(t.get_timeout());
    }

public:
    timer_set() : _last(0) , _next(max_timestamp) { }
    ~timer_set();

    bool insert(timer_handle& h);
    void remove(timer_handle& h);
    timer_set_t expire(time_point tnow);

    void clear();
    size_t size() const { return _buckets.size(); }
    bool empty() const { return (_buckets.size()==0); }
    time_point now() { return Clock::now(); }
    time_point get_next_timeout() const {
        return time_point(duration(std::max(_last, _next)));
    }

};

class timer_manager : public Singleton<timer_manager>{
public:
private:
    timer_manager();
    ~timer_manager();

    int _timerfd = {};
    timer_set _timers;
    boost::shared_mutex   _timer_mutex;
    boost::shared_mutex   _fd_mutex;
public:
    static void completed(){
        timer_manager::Instance().enable_timer(Clock::now());
        close(timer_manager::Instance()._timerfd);
    }
public:
    std::shared_ptr<thread_pool> _thread_pool;
    void set_thread_pool(std::shared_ptr<thread_pool> pool);

    void complete_timers();    
    void enable_timer(time_point when);
public:
    timer_handle add_timer(duration delta, callback_t&& callback);
    timer_handle add_timer(time_point when, duration delta, callback_t&& callback);
    void del_timer(timer_handle& ret);

    friend class timer;
    friend class timer_set;
    friend class timer_handle;
    friend class Singleton<timer_manager>;
};





