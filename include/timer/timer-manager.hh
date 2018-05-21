#pragma once

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <atomic>
#include <unordered_map>

#include "singleton.hh"
#include "timer.hh"
#include "timer-set.hh"
#include "thread_pool.hh"
#include "signal-manager.hh"


class timer_manager : public Singleton<timer_manager>{
    timer_t _steady_clock_timer = {};
    timer_set<timer<>, &timer<>::_link> _timers;
    timer_set<timer<>, &timer<>::_link>::timer_list_t _expired_timers;

    timer_manager();
    ~timer_manager();
private:

public:
private:
    template <typename T, typename E, typename EnableFunc>
        void complete_timers(T& timers, E& expired_timers, EnableFunc&& enable_fn);
    void enable_timer(steady_clock_type::time_point when);
    bool queue_timer(timer<steady_clock_type>* tmr);
public:
    void add_timer(timer<steady_clock_type>* tmr);
    void del_timer(timer<steady_clock_type>* tmr);
    void expired_timer(timer<steady_clock_type>::duration delta, timer<steady_clock_type>::callback_t &&callback);

    friend class timer<>;
    friend class Singleton<timer_manager>;
};

void timer_manager::enable_timer(steady_clock_type::time_point when)
{
    itimerspec its;
    its.it_interval = {};
    its.it_value = to_timespec(when);
    auto ret = timer_settime(_steady_clock_timer, TIMER_ABSTIME, &its, NULL);
    throw_system_error_on(ret == -1);
}


void timer_manager::expired_timer(timer<steady_clock_type>::duration delta, timer<steady_clock_type>::callback_t &&callback)
{
    timer<steady_clock_type>* pt = new timer<steady_clock_type>(std::move(callback), true);
    pt->arm(delta);
}

void timer_manager::add_timer(timer<steady_clock_type>* tmr)
{
    if (queue_timer(tmr)) {
        enable_timer(_timers.get_next_timeout());
    }
}

bool timer_manager::queue_timer(timer<steady_clock_type>* tmr)
{
    return _timers.insert(*tmr);
}

void timer_manager::del_timer(timer<steady_clock_type>* tmr)
{
    if (tmr->_expired) {
        _expired_timers.erase(_expired_timers.iterator_to(*tmr));
        tmr->_expired = false;
    } else {
        _timers.remove(*tmr);
    }
    if (tmr->_need_disposer)
        tmr.~timer();
}

timer_manager::timer_manager()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, alarm_signal());
    auto r = ::pthread_sigmask(SIG_BLOCK, &mask, NULL);
    assert( r == 0 );

    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev._sigev_un._tid = syscall(SYS_gettid);
    sev.sigev_signo = alarm_signal();
    r = timer_create(CLOCK_MONOTONIC, &sev, &_steady_clock_timer);
    assert(r == 0);

    singal_manager::Instance().handle_signal(alarm_signal(), [this] {
            complete_timers(_timers, _expired_timers, [this] {
                if (!_timers.empty()){
                    enable_timer(_timers.get_next_timeout());
                }
            });
    });
}


template <typename T, typename E, typename EnableFunc>
void timer_manager::complete_timers(T& timers, E& expired_timers, EnableFunc&& enable_fn) {
    expired_timers = timers.expire(timers.now());
    for (auto& t : expired_timers) {
        t._expired = true;
    }
    using timer_type = typename T::timer_t;
    while (!expired_timers.empty()) {
        timer_type* t = &*expired_timers.begin();
        expired_timers.pop_front();
        t->_queued = false;
        if (t->_armed) {
            t->_armed = false;
            if (t->_period) {
                t->readd_periodic();
            }
            if (t->_need_disposer)
                std::shared_ptr<timer_type> p(t);
            try {
                t->_callback();
            } catch (...) {
                std::runtime_error("Timer callback failed: {}");
            }
        }
    }
    enable_fn();
    return;
}


timer_manager::~timer_manager()
{
    timer_delete(_steady_clock_timer);
    auto eraser = [](timer_set<timer<>, &timer<>::_link>::timer_list_t& list){
        while(!list.empty()){
            auto& timer = *list.begin();
            timer.cancel();
        }
    };

    eraser(_expired_timers);
}

template <typename Clock>
inline
timer<Clock>::timer(callback_t&& callback, bool need_disposer) : _callback(std::move(callback)), _need_disposer(need_disposer) {
}

template <typename Clock>
inline
timer<Clock>::~timer() {
    if (_queued) {
        timer_manager::Instance().del_timer(this);
    }
}

template <typename Clock>
inline
void timer<Clock>::set_callback(callback_t&& callback) {
    _callback = std::move(callback);
}

template <typename Clock>
inline
void timer<Clock>::arm_state(time_point until, boost::optional<duration> period) {
    assert(!_armed);
    _period = period;
    _armed = true;
    _expired = false;
    _expiry = until;
    _queued = true;
}

template <typename Clock>
inline
void timer<Clock>::arm(time_point until, boost::optional<duration> period) {
    arm_state(until, period);
    timer_manager::Instance().add_timer(this);
}

template <typename Clock>
inline
void timer<Clock>::rearm(time_point until, boost::optional<duration> period) {
    if (_armed) {
        cancel();
    }
    arm(until, period);
}

template <typename Clock>
inline
void timer<Clock>::arm(duration delta) {
    return arm(Clock::now() + delta);
}

template <typename Clock>
inline
void timer<Clock>::arm_periodic(duration delta) {
    arm(Clock::now() + delta, {delta});
}

template <typename Clock>
inline
void timer<Clock>::readd_periodic() {
    //arm_state(Clock::now() + _period.value(), {_period.value()});
    arm_state(Clock::now() + _period.get(), {_period.get()});
    timer_manager::Instance().queue_timer(this);
}

template <typename Clock>
inline
bool timer<Clock>::cancel() {
    if (!_armed) {
        return false;
    }
    _armed = false;
    if (_queued) {
        timer_manager::Instance().del_timer(this);
        _queued = false;
    }
    return true;
}

template <typename Clock>
inline
typename timer<Clock>::time_point timer<Clock>::get_timeout() {
    return _expiry;
}



