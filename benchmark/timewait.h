#pragma once
#include <chrono>

/**
 * Time wait for the runnable object
 * Use std::milli or std::micro or std::nano to fill template parameter
 */
template <typename Precision, typename F, typename... Args>
static double timewait(F&& foo, Args&&... argv) {
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, Precision>(time_end - time_start).count();
}

/**
 * Time wait for the runnable object
 * And the precision is std::chrono::second
 */
template <typename F, typename... Args>
static double timewait(F&& foo, Args&&... argv) {
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(time_end - time_start).count();
}

