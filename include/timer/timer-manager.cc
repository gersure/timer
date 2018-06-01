#include "time.h"
#include "timer-manager.hh"

std::atomic<timer_id> timer::g_timer_id(0);

timer_set::~timer_set() {
    while (!_buckets.empty()) {
        auto h = _buckets.begin();
        _buckets.erase(h);
        delete h->_ptimer;
    }
}

void timer_set::clear() {
    while (!_buckets.empty()) {
        auto h = _buckets.begin();
        _buckets.erase(h);
        delete h->_ptimer;
    }
}

bool timer_set::insert(timer_handle& h)
{
    auto timestamp = get_timestamp(*(h._ptimer));
    _buckets.insert(h);

    if (timestamp < _next) {
        _next = timestamp;
        return true;
    }
    return false;
}


void timer_set::remove(timer_handle& h)
{
    auto it1 = _buckets.lower_bound(h);
    auto it2 = _buckets.upper_bound(h);
    for(timer_set_t::iterator it = it1; it != it2; it++){
        if(it->_ptimer->get_id() == h._id){
            _buckets.erase(it);
            return;
        }
    }
}

timer_set_t timer_set::expire(time_point tnow)
{
    timer t;
    timer_set_t exp;
    t._expiry = tnow;

    auto timestamp = get_timestamp(tnow);

    if (timestamp < _last) {
        abort();
    }

    auto it = _buckets.upper_bound({&t, t.get_id()});
    exp.insert(_buckets.begin(), it);
    _buckets.erase(_buckets.begin(), it);
    _last = timestamp;
    if (_buckets.size())
        _next = get_timestamp(*(_buckets.begin()->_ptimer));

    return exp;
}


void timer_manager::enable_timer(time_point when)
{
    itimerspec its, old;
    its.it_interval = {};
    its.it_value = to_timespec(when);
    auto ret = timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &its, &old);
    throw_system_error_on(ret == -1);
}

timer_handle  timer_manager::add_timer(duration delta, callback_t&& callback)
{
    timer *p = new timer(delta, std::move(callback));
    timer_handle h( p, p->get_id());
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    if (_timers.insert(h))
        enable_timer(_timers.get_next_timeout());
    return h;
}

timer_handle timer_manager::add_timer(time_point when, duration delta, callback_t&& callback)
{
    timer* p = new timer;
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
    boost::unique_lock<boost::shared_mutex> u_timers(_timer_mutex);
    _timers.remove(h);
}

timer_manager::timer_manager()
{
    _timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    throw_system_error_on(_timerfd == (-1), strerror(errno));
}


void timer_manager::complete_timers() {
    uint64_t  expire = {};
    read(_timerfd, &expire, sizeof(expire));
    timer_set_t  expired_timers;
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
                t.get_timer()->arm(Clock::now() + t.get_timer()->_period.get(), {t.get_timer()->_period.get()});
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
        if (!_thread_pool){
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

