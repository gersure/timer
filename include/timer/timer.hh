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
    typedef typename Clock::time_point time_point;
    typedef typename Clock::duration duration;
    typedef Clock clock;
private:
    using callback_t = std::function<void()>;
    boost::intrusive::list_member_hook<> _link;
    callback_t _callback;
    time_point _expiry;
    boost::optional<duration> _period;
    bool _armed = false;
    bool _queued = false;
    bool _expired = false;
    void readd_periodic();
    void arm_state(time_point until, boost::optional<duration> period);
public:
    timer() = default;
    timer(timer&& t) noexcept : _callback(std::move(t._callback)), _expiry(std::move(t._expiry)), _period(std::move(t._period)),
            _armed(t._armed), _queued(t._queued), _expired(t._expired) {
        _link.swap_nodes(t._link);
        t._queued = false;
        t._armed = false;
    }
    explicit timer(callback_t&& callback);
    ~timer();
    void set_callback(callback_t&& callback);
    void arm(time_point until, boost::optional<duration> period = {});
    void rearm(time_point until, boost::optional<duration> period = {});
    void arm(duration delta);
    void arm_periodic(duration delta);
    bool armed() const { return _armed; }
    bool cancel();
    time_point get_timeout();
    friend class timer_manager;
    friend class timer_set<timer, &timer::_link>;
};

