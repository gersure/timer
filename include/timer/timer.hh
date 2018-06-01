#pragma once

#include <chrono>
#include <atomic>
#include <functional>
#include "timer-set.hh"
#include "boost/optional.hpp"

using steady_clock_type = std::chrono::steady_clock;

template <typename Clock = steady_clock_type>
class timer {
public:
    typedef uint64_t timer_id;
    typedef Clock clock;
    typedef typename Clock::duration duration;
    typedef typename Clock::time_point time_point;
    using callback_t = std::function<void()>;
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

    timer(const timer& t) noexcept;
    timer(timer&& t) noexcept;

    void set_callback(callback_t&& callback);
    callback_t get_callback( ) { return _callback; }
    void arm(duration delta);
    void arm(time_point until, boost::optional<duration> period = {});

    bool armed() const { return _armed; }
    timer_id get_id() { return _timer_id; }
    time_point get_timeout() { return _expiry; }

    friend class timer_manager;
    friend class timer_set<timer>;
};

template <typename Clock>
std::atomic<uint64_t> timer<Clock>::g_timer_id(0);

template <typename Clock>
timer<Clock>::timer(duration delta, callback_t&& callback, timer_id timeid)
    : _callback(std::move(callback)), _timer_id(timeid)
{
//    arm(delta);
    _expiry = Clock::now()+delta;
    _armed = true;
    _expired = false;
}

template <typename Clock>
timer<Clock>::timer(const timer& t) noexcept : _callback(t._callback) , _expiry(t._expiry)
    , _period(t._period) , _armed(t._armed), _expired(t._expired) , _timer_id(t._timer_id)
{
}

template <typename Clock>
timer<Clock>::timer(timer&& t) noexcept : _callback(t._callback) , _expiry(t._expiry)
    , _period(t._period) , _armed(t._armed), _expired(t._expired) , _timer_id(t._timer_id)
{
    t._armed = false;
}

template <typename Clock>
inline void timer<Clock>::set_callback(callback_t&& callback) {
    _callback = std::move(callback);
}

template <typename Clock>
inline void timer<Clock>::arm(duration delta) {
    return arm(Clock::now() + delta);
}

template <typename Clock>
inline void timer<Clock>::arm(time_point until, boost::optional<duration> period) {
    _period = period;
    _expiry = until;
    _armed = true;
    _expired = false;
}


