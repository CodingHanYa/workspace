#pragma once
#include <map>
#include <memory>
#include <vector>

#include "Timer.h"
#include "TimerScheduler.h"

namespace hipe {
template <typename T>
class RbtreeTimerScheduler : public TimerScheduler<T> {
    using TimerScheduler<T>::mtx;
    using TimerScheduler<T>::running;
    using TimerScheduler<T>::cv;
    using TimerScheduler<T>::thread_pool;

private:
    std::multimap<steady_clock::time_point, std::unique_ptr<Timer>> timer_map{};  // trigger time, timer pointer
    std::vector<std::pair<steady_clock::time_point, std::unique_ptr<Timer>>> temp_timers;  // 临时存放timer_map中的元素
    std::vector<std::pair<steady_clock::time_point, std::unique_ptr<Timer>>> expired_timers;
    std::unordered_map<size_t, std::reference_wrapper<Timer>> cancelling_map;

private:
    bool cancelling{false};
    bool revise{false};
    Timer* current{};

public:
    RbtreeTimerScheduler() = default;
    explicit RbtreeTimerScheduler(bool revise_)
      : revise{revise_} {
    }
    explicit RbtreeTimerScheduler(T* pool)
      : TimerScheduler<T>{pool} {
    }
    RbtreeTimerScheduler(T* pool, bool revise_)
      : TimerScheduler<T>{pool}
      , revise{revise_} {
    }
    ~RbtreeTimerScheduler() override = default;

private:
    void init_timers() override {
        const auto now{steady_clock::now()};

        for (auto& timer : temp_timers) timer.first = timer.second->init_trigger_time(now);

        timer_map.insert(make_move_iterator(begin(temp_timers)), make_move_iterator(end(temp_timers)));
        temp_timers.clear();
        temp_timers.shrink_to_fit();
    }

    void schedule() noexcept override {
        std::unique_lock<std::mutex> lock{mtx};

        while (running) {
            if (timer_map.empty()) {
                cv.wait(lock);
                continue;
            }

            const auto now{steady_clock::now()};
            const auto remaining_sleep_time{begin(timer_map)->first - now};

            if (remaining_sleep_time <= microseconds::zero()) {
                auto end_iter{timer_map.upper_bound(now)};
                copy(make_move_iterator(begin(timer_map)), make_move_iterator(end_iter), back_inserter(expired_timers));
                timer_map.erase(begin(timer_map), end_iter);

                if (expired_timers.empty()) {
                    std::cerr << "expired timers is empty while remaining_sleep_time <= zero" << std::endl;
                }

                for (auto& pair : expired_timers) {
                    auto& tp{pair.first};
                    auto& timer{pair.second};
                    if (!timer->cancelled() && timer->repeatable()) {
                        if (revise)
                            tp = timer->dynamic_update_trigger_time(now);
                        else
                            tp = timer->steady_update_trigger_time();
                    }
                }

                for (
                    const auto& pair :
                    expired_timers)  // Timer的回调函数可能被延时执行。例如a，b两个timer都在第2秒超时，a先执行，耗时1秒，b就只能在第3秒的时候执行。
                {
                    const auto& timer{pair.second};

                    if (timer->cancelled()) {
                        continue;
                    }

                    current = timer.get();

                    lock.unlock();

                    if (thread_pool != nullptr) {
                        thread_pool->submit(*timer);  // todo:如何减少timer对象的拷贝？
                    } else {
                        timer->trigger();
                    }
                    lock.lock();

                    current = nullptr;

                    if (timer->cancelled()) {
                        cancelling = false;
                        cv.notify_all();
                    }
                }

                for (auto& pair : expired_timers) {
                    auto& timer{pair.second};
                    if (!timer->cancelled() && timer->repeatable()) {
                        if (running)
                            timer_map.emplace(std::move(pair));
                        else
                            temp_timers.emplace_back(std::move(pair));
                    } else
                        cancelling_map.erase(timer->get_id());
                }
                expired_timers.clear();
            } else {
                cv.wait_for(lock, remaining_sleep_time);
            }
        }
    }

public:
    size_t add_timer(std::function<void()> cb, microseconds delay, microseconds interval) override {
        auto timer{std::unique_ptr<Timer>{new Timer{std::move(cb), delay, interval}}};
        size_t timer_id{timer->get_id()};

        std::lock_guard<std::mutex> guard{mtx};

        cancelling_map.emplace(timer->get_id(), *timer);

        if (running) {
            timer->init_trigger_time(steady_clock::now());
            timer_map.emplace(timer->get_trigger_time(), std::move(timer));
            cv.notify_one();  // 新定时器的触发时间可能更早
        } else {
            temp_timers.emplace_back(microseconds::zero(), std::move(timer));
        }

        return timer_id;
    }

    bool cancel(size_t timer_id, bool wait) override {
        std::unique_lock<std::mutex> lock{mtx};

        auto it{cancelling_map.find(timer_id)};
        if (it != end(cancelling_map) && !it->second.get().cancelled()) {
            it->second.get().cancel();
            cancelling_map.erase(it);

            if (current && current->get_id() == timer_id) {
                if (wait) {
                    cancelling = true;
                    cv.wait(lock, [this]() { return !cancelling; });
                }
            }

            return true;
        }

        return false;
    }
};

}  // namespace hipe
