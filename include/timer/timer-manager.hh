#pragma once

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <atomic>
#include <unordered_map>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>

#include "posix.hh"
#include "singleton.hh"
#include "timer.hh"
#include "timer-set.hh"
#include "thread_pool.hh"


inline int alarm_signal()
{
    return SIGRTMIN;
}

inline sigset_t make_sigset_mask(int signo)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signo);
    return set;
}

inline sigset_t make_full_sigset_mask()
{
    sigset_t set;
    sigfillset(&set);
    return set;
}

inline sigset_t make_empty_sigset_mask()
{
    sigset_t set;
    sigemptyset(&set);
    return set;
}


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
    timer_t  _steady_clock_timer = {};
    timer_set<mtimer_t> _timers;
    boost::shared_mutex   _timer_mutex;
private:
    timer_manager();
    ~timer_manager();

    class signals{
        public:
            signals();
            ~signals();

            bool poll_signal();
            bool pure_poll_signal() const;
            void handle_signal(int signo, std::function<void ()>&& handler);
            void handle_signal_once(int signo, std::function<void ()>&& handler);
            static void action(int signo, siginfo_t* siginfo, void* ignore);
        private:

            struct signal_handler {
                signal_handler(int signo, std::function<void ()>&& handler);
                std::function<void ()> _handler;
            };
            std::atomic<uint64_t> _pending_signals;
            std::unordered_map<int, signal_handler> _signal_handlers;
    };
public:
    signals _signals;
    std::shared_ptr<thread_pool> _thread_pool;
    void set_thread_pool(std::shared_ptr<thread_pool> pool);

    template <typename T, typename EnableFunc>
        void complete_timers(T& timers, EnableFunc&& enable_fn);
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
    itimerspec its;
    its.it_interval = {};
    its.it_value = to_timespec(when);
    auto ret = timer_settime(_steady_clock_timer, TIMER_ABSTIME, &its, NULL);
    throw_system_error_on(ret == -1);
}

timer_manager::timer_ret  timer_manager::add_timer(duration delta, callback_t&& callback)
{
    std::pair<bool, timer_index> ret;
    mtimer_t t(delta, std::move(callback));
    {
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    ret = _timers.insert(t);
    }
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
    {
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    ret = _timers.insert(t);
    }
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

    _signals.handle_signal(alarm_signal(), [this] {
            complete_timers(_timers, [this] {
                if (!_timers.empty()){
                    enable_timer(_timers.get_next_timeout());
                }
            });
    });
}


template <typename T, typename EnableFunc>
void timer_manager::complete_timers(T& timers, EnableFunc&& enable_fn) {
    typename T::timer_list_t  expired_timers;
    //{
        boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
        expired_timers = timers.expire(timers.now());
    //}
    std::cout<<expired_timers.size()<<std::endl;
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
                t.second._callback();
            } catch (...) {
                std::runtime_error("Timer callback failed: {}");
            }
        }
    }
    enable_fn();
    return;
}


timer_manager::~timer_manager()
{}

void timer_manager::set_thread_pool(std::shared_ptr<thread_pool> pool)
{
    _thread_pool.swap(pool);
}

/**********************************/
timer_manager::signals::signals() : _pending_signals(0)
{ }

timer_manager::signals::~signals() {
    sigset_t mask;
    sigfillset(&mask);
    ::pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

timer_manager::signals::signal_handler::signal_handler(int signo, std::function<void ()>&& handler)
        : _handler(std::move(handler)) {
    struct sigaction sa;
    sa.sa_sigaction = action;
    sa.sa_mask = make_empty_sigset_mask();
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    auto r = ::sigaction(signo, &sa, nullptr);
    throw_system_error_on(r == -1);
    auto mask = make_sigset_mask(signo);
    r = ::pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    throw_pthread_error(r);
}

void
timer_manager::signals::handle_signal(int signo, std::function<void ()>&& handler) {
    _signal_handlers.emplace(std::piecewise_construct,
        std::make_tuple(signo), std::make_tuple(signo, std::move(handler)));
}

void
timer_manager::signals::handle_signal_once(int signo, std::function<void ()>&& handler) {
    return handle_signal(signo, [fired = false, handler = std::move(handler)] () mutable {
        if (!fired) {
            fired = true;
            handler();
        }
    });
}

bool timer_manager::signals::poll_signal() {
    auto ato_signals = _pending_signals.load(std::memory_order_relaxed);
    if (ato_signals) {
        _pending_signals.fetch_and(~ato_signals, std::memory_order_relaxed);
        for (size_t i = 0; i < sizeof(ato_signals)*8; i++) {
            if (ato_signals & (1ull << i)) {
                if(timer_manager::Instance()._thread_pool && (!timer_manager::Instance()._thread_pool->stopped())){
                    timer_manager::Instance()._thread_pool->enqueue(_signal_handlers.at(i)._handler);
                }
                else
                    _signal_handlers.at(i)._handler();
            }
        }
    }
    return ato_signals;
}

bool timer_manager::signals::pure_poll_signal() const {
    return _pending_signals.load(std::memory_order_relaxed);
}

void timer_manager::signals::action(int signo, siginfo_t* siginfo, void* ignore) {
    std::cout<<"action..."<<std::endl;
    timer_manager::Instance()._signals._pending_signals.fetch_or(1ull << signo, std::memory_order_relaxed);
    if ((timer_manager::Instance()._thread_pool)){
        timer_manager::Instance()._thread_pool->notify_monitor();
    }
}

void thread_pool::recycle()
{
    eventfd_t  value = 0;
    while (!stop.load(std::memory_order_relaxed))
    {
        auto ret = read(monitorfd, &value, sizeof(value));
        if (value != 1 || ret != sizeof(value)){
            continue;
        }
        if (timer_manager::Instance()._signals.pure_poll_signal()){
            timer_manager::Instance()._signals.poll_signal();
        }
    }
}

