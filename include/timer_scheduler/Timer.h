#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace hipe {

using std::chrono::microseconds;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::steady_clock;

struct Timer {
private:
    std::function<void()> callback;
    microseconds start_delay;
    microseconds interval;                    // 每隔该时间触发一次；该值为0表示不重复触发
    steady_clock::time_point trigger_time{};  // 下一次的触发时间
    bool invalid{false};
    size_t id;                                // 为了使Timer可移动，id不能为const
    static std::atomic<size_t> sequence_generator;

public:
    Timer(std::function<void()> cb, microseconds delay, microseconds itv = microseconds::zero())
      : callback{std::move(cb)}
      , start_delay{delay}
      , interval{itv}
      , id{++sequence_generator} {
    }

    Timer(const Timer& other) = default;
    Timer& operator=(const Timer& other) = default;
    Timer(Timer&& other) noexcept = default;
    Timer& operator=(Timer&& other) noexcept = default;

public:
    bool repeatable() const {
        return interval != microseconds::zero();
    }

    void cancel() {
        invalid = true;
    }

    bool cancelled() const {
        return invalid;
    }

    steady_clock::time_point init_trigger_time(steady_clock::time_point now) {
        trigger_time = now + start_delay;
        return trigger_time;
    }

    steady_clock::time_point dynamic_update_trigger_time(steady_clock::time_point now) {
        if (!repeatable()) {
            throw std::logic_error{"unrepeatable timer cannot restart!"};
        }

        trigger_time = now + interval;
        return trigger_time;
    }

    steady_clock::time_point steady_update_trigger_time() {
        if (!repeatable()) {
            throw std::logic_error{"unrepeatable timer cannot restart!"};
        }

        trigger_time += interval;
        return trigger_time;
    }


    void trigger() const noexcept {
        try {
            callback();
        } catch (const std::exception& e) {
            std::cerr << "timer callback throws an std exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "timer callback throws an unknown exception." << std::endl;
        }
    }

    void operator()() const noexcept {
        trigger();
    }

    bool operator<(const Timer& other) const {
        return this->trigger_time < other.trigger_time;
    }
    bool operator>(const Timer& other) const {
        return this->trigger_time > other.trigger_time;
    }

    size_t get_id() const {
        return id;
    }
    steady_clock::time_point get_trigger_time() const {
        return trigger_time;
    }
};

std::atomic<size_t> Timer::sequence_generator{0};
}  // namespace hipe
