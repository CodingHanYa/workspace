#pragma once
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Timer.h"
#include "TimerScheduler.h"

namespace hipe {
template <typename T>
class HeapTimerScheduler : public TimerScheduler<T> {
    using TimerScheduler<T>::mtx;
    using TimerScheduler<T>::running;
    using TimerScheduler<T>::cv;
    using TimerScheduler<T>::thread_pool;

private:
    std::vector<std::unique_ptr<Timer>> timers{};
    std::unordered_map<size_t, std::reference_wrapper<Timer>> cancelling_map{};  // timer id, timer
    static bool greater_comp(const std::unique_ptr<Timer>& l, const std::unique_ptr<Timer>& r) {
        return *l > *r;
    }

private:
    Timer* current{nullptr};
    bool cancelling{false};

private:
    bool revise{false};

    void init_timers() override {
        const auto now{steady_clock::now()};
        for (auto& t : timers) t->init_trigger_time(now);

        make_heap(begin(timers), end(timers), greater_comp);
    }

public:
    HeapTimerScheduler() = default;
    explicit HeapTimerScheduler(bool revise_)
      : revise{revise_} {
    }
    explicit HeapTimerScheduler(T* pool)
      : TimerScheduler<T>{pool} {
    }
    explicit HeapTimerScheduler(T* pool, bool revise_)
      : TimerScheduler<T>{pool}
      , revise{revise_} {
    }
    ~HeapTimerScheduler() override = default;

private:
    size_t add_timer(std::function<void()> cb, microseconds delay, microseconds interval) override {
        auto timer{std::unique_ptr<Timer>{new Timer{std::move(cb), delay, interval}}};
        const auto timer_id{timer->get_id()};

        std::lock_guard<std::mutex> guard{mtx};

        timers.emplace_back(std::move(timer));
        cancelling_map.emplace(timer_id, *timers.back());

        if (running) {
            timers.back()->init_trigger_time(steady_clock::now());
            push_heap(begin(timers), end(timers), greater_comp);
            cv.notify_one();  // 新定时器的触发时间可能最早
        }

        return timer_id;
    }

public:
    bool cancel(size_t timer_id, bool wait) override {
        std::unique_lock<std::mutex> lock{mtx};

        // 无法将定时器从堆中移除，否则将破坏堆的结构
        // 只能够将定时器标记为失效，令调度器调度到该定时器时再移除
        auto it{cancelling_map.find(timer_id)};
        if (it != cancelling_map.end() && !it->second.get().cancelled()) {
            it->second.get().cancel();
            cancelling_map.erase(it);

            if (current && current->get_id() == timer_id && wait) {
                cancelling = true;
                cv.wait(lock, [this]() { return !cancelling; });
            }

            return true;
        }

        return false;
    }

private:
    void run_callback(std::unique_lock<std::mutex>& lock, steady_clock::time_point now) {
        std::unique_ptr<Timer> timer{std::move(timers.back())};
        timers.pop_back();

        if (timer->cancelled()) {
            std::cerr << "timer whose id = " << timer->get_id() << " has been cancelled." << std::endl;
            return;
        }

        current = timer.get();
        if (current->repeatable())  // 在执行回调函数前，计算下次触发时间
        {
            if (revise)
                timer->dynamic_update_trigger_time(now);
            else
                timer->steady_update_trigger_time();
        }

        lock.unlock();
        if (thread_pool != nullptr) {
            thread_pool->submit(*timer);  // todo:如何减少timer对象的拷贝？
        } else {
            timer->trigger();
        }
        lock.lock();
        current = nullptr;

        if (timer->cancelled())  // 在执行定时器回调函数时，可能取消了该重复触发的定时器，不能再次将该定时器加入队列。
        {
            cancelling = false;
            cv.notify_all();
        }

        if (!timer->cancelled() && timer->repeatable()) {
            timers.push_back(std::move(timer));

            if (running) push_heap(begin(timers), end(timers), greater_comp);
        } else {
            cancelling_map.erase(timer->get_id());
        }
    }

    void schedule() noexcept override {
        std::unique_lock<std::mutex> lock{mtx};

        while (running) {
            if (timers.empty()) {
                cv.wait(lock);
                continue;  // 除添加定时器外，关闭调度器、取消定时器也会唤醒
            }

            const auto now{steady_clock::now()};
            const auto remaining_sleep_time{timers.front()->get_trigger_time() - now};

            if (timers.front()->cancelled()) {
                pop_heap(begin(timers), end(timers), greater_comp);
                timers.pop_back();
                continue;  // 定时器队列可能为空
            }

            if (remaining_sleep_time <= microseconds::zero()) {
                pop_heap(begin(timers), end(timers), greater_comp);
                run_callback(lock, now);
            } else {
                // 调度器中的第一个定时器执行此分支，睡眠直到定时器超时
                // 调度器被错误地唤醒，（例如添加新定时器，且新定时器的触发时间大于旧定时器）睡眠剩余的时间
                cv.wait_for(lock, remaining_sleep_time);
            }
        }
    }
};

}  // namespace hipe
