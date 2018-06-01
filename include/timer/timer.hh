#pragma once

#include <chrono>
#include <atomic>
#include <functional>
#include <boost/noncopyable.hpp>
#include "boost/optional.hpp"
#include "timer_types.hh"


class timer : boost::noncopyable{
public:
private:
    callback_t _callback;
    time_point _expiry;
    boost::optional<duration> _period = boost::none;
    bool _armed = false;
    bool _expired = false;
    timer_id   _timer_id  = 0;

    static std::atomic<timer_id> g_timer_id;
    static timer_id get_timeid(){return (g_timer_id++);}
public:
    explicit timer(timer_id timeid=timer::get_timeid()):_timer_id(timeid) {  }
    explicit timer(duration delta, callback_t&& callback, timer_id timeid=timer::get_timeid());
    ~timer() {}

public:
    void set_callback(callback_t&& callback);
    callback_t get_callback( ) { return _callback; }
    void arm(duration delta);
    void arm(time_point until, boost::optional<duration> period = {});

    bool armed() const { return _armed; }
    timer_id get_id() { return _timer_id; }
    time_point get_timeout() { return _expiry; }

    friend class timer_set;
    friend class timer_handle;
    friend class timer_manager;
};


class timer_handle
{
public:
    friend class timer;
    friend class timer_set;
    typedef uint64_t timer_id;
    timer_handle(timer* t, timer_id seq)
        : _id(seq),
          _ptimer(t)
    {
    }

    bool operator < (const timer_handle& h) const {
        return _ptimer->_expiry < h._ptimer->_expiry;
    }

    timer*  get_timer(){ return _ptimer; }
private:
    timer_id  _id;
    timer*  _ptimer;
};

std::atomic<uint64_t> timer::g_timer_id(0);

timer::timer(duration delta, callback_t&& callback, timer_id timeid)
    : _callback(std::move(callback)), _timer_id(timeid)
{
    arm(delta);
}

inline void timer::set_callback(callback_t&& callback) {
    _callback = std::move(callback);
}

inline void timer::arm(duration delta) {
    return arm(Clock::now() + delta);
}

inline void timer::arm(time_point until, boost::optional<duration> period) {
    _period = period;
    _expiry = until;
    _armed = true;
    _expired = false;
}



