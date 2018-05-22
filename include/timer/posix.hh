#pragma once

#include <chrono>
#include <system_error>

using steady_clock_type = std::chrono::steady_clock;

inline void throw_system_error_on(bool condition, const char* what_arg="")
//inline void throw_system_error_on(bool condition, const char* what_arg)
{
    if (condition) {
        throw std::system_error(errno, std::system_category(), what_arg);
    }
}

template <typename T>
inline void throw_pthread_error(T r)
{
    if (r != 0) {
        throw std::system_error(r, std::system_category());
    }
}



timespec to_timespec(steady_clock_type::time_point t)
{
    using ns = std::chrono::nanoseconds;
    auto n = std::chrono::duration_cast<ns>(t.time_since_epoch()).count();
    return { n / 1000000000, n % 1000000000 };
}


