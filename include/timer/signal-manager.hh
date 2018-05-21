#pragma once

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <atomic>
#include <unordered_map>

#include "posix.hh"
#include "singleton.hh"
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


class singal_manager : public Singleton<singal_manager>{
    public:
        friend Singleton<singal_manager>;
        //            bool poll_signal();
        //            bool pure_poll_signal() const;
        void handle_signal(int signo, std::function<void ()>&& handler);
        void handle_signal_once(int signo, std::function<void ()>&& handler);
        static void action(int signo, siginfo_t* siginfo, void* ignore);
    private:
        singal_manager();
        ~singal_manager();

        struct signal_handler {
            signal_handler(int signo, std::function<void ()>&& handler);
            std::function<void ()> _handler;
        };
        //std::atomic<uint64_t> _pending_singal_manager;
        std::unordered_map<int, signal_handler> _signal_handlers;
};

singal_manager::singal_manager() //: _pending_singal_manager(0) {
{ }

singal_manager::~singal_manager() {
    sigset_t mask;
    sigfillset(&mask);
    ::pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

singal_manager::signal_handler::signal_handler(int signo, std::function<void ()>&& handler)
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
singal_manager::handle_signal(int signo, std::function<void ()>&& handler) {
    _signal_handlers.emplace(std::piecewise_construct,
        std::make_tuple(signo), std::make_tuple(signo, std::move(handler)));
}

void
singal_manager::handle_signal_once(int signo, std::function<void ()>&& handler) {
    return handle_signal(signo, [fired = false, handler = std::move(handler)] () mutable {
        if (!fired) {
            fired = true;
            handler();
        }
    });
}

//bool timer_manager::singal_manager::poll_signal() {
//    auto ato_singal_manager = _pending_singal_manager.load(std::memory_order_relaxed);
//    if (ato_singal_manager) {
//        _pending_singal_manager.fetch_and(~ato_singal_manager, std::memory_order_relaxed);
//        for (size_t i = 0; i < sizeof(ato_singal_manager)*8; i++) {
//            if (ato_singal_manager & (1ull << i)) {
//               _signal_handlers.at(i)._handler();
//            }
//        }
//    }
//    return ato_singal_manager;
//}
//
//bool timer_manager::singal_manager::pure_poll_signal() const {
//    return _pending_singal_manager.load(std::memory_order_relaxed);
//}

void singal_manager::action(int signo, siginfo_t* siginfo, void* ignore) {
    //timer_manager::Instance()._singal_manager._pending_singal_manager.fetch_or(1ull << signo, std::memory_order_relaxed);
    thread_pool::Instance().enqueue(singal_manager::Instance()._signal_handlers.at(signo)._handler);
}


