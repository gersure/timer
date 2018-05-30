#ifndef __TIMER_SET_HH
#define __TIMER_SET_HH

#include <chrono>
#include <limits>
#include <bitset>
#include <array>
#include <cmath>
#include <boost/optional.hpp>
#include <boost/intrusive/list.hpp>
#include "bitset-iter.hh"

template<typename Timer>
class timer_set {
public:
    //using timer_t = typename Timer;
    using timer_index = int;
    using timer_id = typename Timer::timer_id;
    using time_point   = typename Timer::time_point;
    using timer_list_t = std::unordered_map<timer_id, Timer>;
private:
    using duration = typename Timer::duration;
    using timestamp_t = typename Timer::duration::rep;

    static constexpr timestamp_t max_timestamp = std::numeric_limits<timestamp_t>::max();
    static constexpr int timestamp_bits = std::numeric_limits<timestamp_t>::digits;

    static constexpr int n_buckets = timestamp_bits + 1;
    static constexpr double n_index_bits = std::ceil(std::sqrt(n_buckets));

    std::array<timer_list_t, n_buckets> _buckets;
    timestamp_t _last;
    timestamp_t _next;

    std::bitset<n_buckets> _non_empty_buckets;
private:
    static timestamp_t get_timestamp(time_point _time_point)
    {
        return _time_point.time_since_epoch().count();
    }

    static timestamp_t get_timestamp(Timer& timer)
    {
        return get_timestamp(timer.get_timeout());
    }

    int get_index(timestamp_t timestamp) const
    {
        if (timestamp <= _last) {
            return n_buckets - 1;
        }

        auto index = bitsets::count_leading_zeros(timestamp ^ _last);
        assert(index < n_buckets - 1);
        return index;
    }

    int get_index(Timer& timer) const
    {
        return get_index(get_timestamp(timer));
    }

    int get_last_non_empty_bucket() const
    {
        return bitsets::get_last_set(_non_empty_buckets);
    }
public:
    timer_set()
        : _last(0)
        , _next(max_timestamp)
        , _non_empty_buckets(0)
    {
    }

    ~timer_set() {
        for (auto&& list : _buckets) {
            if (!list.empty()) {
                list.clear();
            }
        }
    }

    boost::optional<timer_index> insert(Timer& timer)
    {
        auto timestamp = get_timestamp(timer);
        auto index = get_index(timestamp);

        _buckets[index].insert({timer.get_id(), timer});
        _non_empty_buckets[index] = true;

        if (timestamp < _next) {
            _next = timestamp;
            return {index};
        }
        return {boost::none};
    }

    void remove(const std::pair<timer_id, timer_index>& ret)
    {
        auto index = ret.second;
        auto& list = _buckets[index];
        auto search = list.find(ret.first);
        if (search != list.end()){
            list.erase(ret.first);
            if (list.empty()) {
                _non_empty_buckets[index] = false;
            }
        }
    }

    timer_list_t expire(time_point tnow)
    {
        timer_list_t exp;
        auto timestamp = get_timestamp(tnow);

        if (timestamp < _last) {
            abort();
        }

        auto index = get_index(timestamp);

        for (int i : bitsets::for_each_set(_non_empty_buckets, index + 1)) {
            exp.insert( _buckets[i].begin(), _buckets[i].end());
            _buckets[i].clear();
            _non_empty_buckets[i] = false;
        }

        _last = timestamp;
        _next = max_timestamp;

        auto& list = _buckets[index];
        while (!list.empty()) {
            auto timer = list.begin();
            if (timer->second.get_timeout() <= tnow) {
                exp.insert({timer->second.get_id(), timer->second});
                remove({timer->second.get_id(), index});
            }
        }

        _non_empty_buckets[index] = !list.empty();

        if (_next == max_timestamp && _non_empty_buckets.any()) {
            for (auto& timer : _buckets[get_last_non_empty_bucket()]) {
                _next = std::min(_next, get_timestamp(timer.second));
            }
        }
        return exp;
    }

   time_point get_next_timeout() const
    {
        return time_point(duration(std::max(_last, _next)));
    }

   void clear()
    {
        for (int i : bitsets::for_each_set(_non_empty_buckets)) {
            _buckets[i].clear();
        }
    }

    size_t size() const
    {
        size_t res = 0;
        for (int i : bitsets::for_each_set(_non_empty_buckets)) {
            res += _buckets[i].size();
        }
        return res;
    }

    bool empty() const
    {
        return _non_empty_buckets.none();
    }

    time_point now() {
        return Timer::clock::now();
    }
};

#endif
