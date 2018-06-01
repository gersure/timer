#pragma once

#include <chrono>
#include <system_error>
#include "timer-types.hh"

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



inline timespec to_timespec(time_point t)
{
    using ns = std::chrono::nanoseconds;
    auto n = std::chrono::duration_cast<ns>(t.time_since_epoch()).count();
    return { n / 1000000000, n % 1000000000 };
}


