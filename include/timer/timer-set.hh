#ifndef __TIMER_SET_HH
#define __TIMER_SET_HH

#include <chrono>
#include <limits>
#include <bitset>
#include <array>
#include <cmath>
#include <set>
#include <boost/optional.hpp>
#include <boost/intrusive/list.hpp>
#include "posix.hh"
#include "timer.hh"

class timer_handle;
class timer_set {
public:
    friend class timer;
    friend class timer_handle;
    using timer_index = int;
    using timer_set_t = std::multiset<timer_handle>;
private:
    typedef uint64_t timer_id;
    typedef std::chrono::steady_clock clock;
    typedef typename clock::duration duration;
    typedef typename clock::time_point time_point;
    using callback_t = std::function<void()>;
    using timestamp_t = typename duration::rep;

    static constexpr timestamp_t max_timestamp = std::numeric_limits<timestamp_t>::max();
    static constexpr int timestamp_bits = std::numeric_limits<timestamp_t>::digits;

    static constexpr int n_buckets = timestamp_bits + 1;
    static constexpr double n_index_bits = std::ceil(std::sqrt(n_buckets));

    timer_set_t _buckets;
    timestamp_t _last;
    timestamp_t _next;
private:
    static timestamp_t get_timestamp(time_point _time_point)
    {
        return _time_point.time_since_epoch().count();
    }

    static timestamp_t get_timestamp(timer& t)
    {
        return get_timestamp(t.get_timeout());
    }

public:
    timer_set()
        : _last(0)
        , _next(max_timestamp)
    {
    }

    ~timer_set() {
        while (!_buckets.empty()) {
            auto h = _buckets.begin();
            _buckets.erase(h);
            delete h->_ptimer;
        }
    }

    bool insert(timer_handle& h)
    {
        auto timestamp = get_timestamp(*(h._ptimer));
        _buckets.insert(h);

        if (timestamp < _next) {
            _next = timestamp;
            return true;
        }
        return false;
    }

    void remove(timer_handle& h)
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

    timer_set_t expire(time_point tnow)
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

   time_point get_next_timeout() const
    {
        return time_point(duration(std::max(_last, _next)));
    }

   void clear()
    {
       while (!_buckets.empty()) {
           auto h = _buckets.begin();
           _buckets.erase(h);
           delete h->_ptimer;
       }
    }

    size_t size() const
    {
        return _buckets.size();
    }

    bool empty() const
    {
        return (_buckets.size()==0);
    }

    time_point now() {
        return timer::clock::now();
    }
};

#endif
