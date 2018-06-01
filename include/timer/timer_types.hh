#ifndef TIMER_TYPES_HH
#define TIMER_TYPES_HH

#include <inttypes.h>
#include <chrono>
#include <functional>
#include <set>

class timer_handle;

typedef uint64_t timer_id;
typedef std::chrono::steady_clock Clock;
typedef typename Clock::duration duration;
typedef typename Clock::time_point time_point;
using callback_t = std::function<void()>;
using timestamp_t = typename duration::rep;
using timer_set_t = std::multiset<timer_handle>;

#endif // TIMER_TYPES_HH
