#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace hipe {
using std::chrono::microseconds;
using std::chrono::nanoseconds;

template <typename T>
class TimerScheduler {
private:
    steady_clock::time_point start_time{};

protected:
    T* thread_pool{};

protected:
    std::thread trd{};
    mutable std::mutex mtx{};
    std::condition_variable
        cv{};  // 等待的情况：无定时器时、等待定时器超时超时时、取消当前执行的定时器；唤醒的情况：添加新定时器、关闭调度器、取消完毕当前执行的定时器
    bool running{false};

public:
    TimerScheduler() = default;
    explicit TimerScheduler(T* pool)
      : thread_pool{pool} {
    }
    virtual ~TimerScheduler() {
        stop();
    }
    TimerScheduler(const TimerScheduler& rhs) = delete;
    TimerScheduler& operator=(const TimerScheduler& rhs) = delete;

private:
    virtual void init_timers() = 0;
    virtual void schedule() noexcept = 0;

public:
    /**
     * @brief 启动定时器调度器。调用start()前创建的定时器在此刻才被调度
     * @return 调度器已启动时返回false
     */
    bool start() {
        std::lock_guard<std::mutex> guard{mtx};

        if (running) return false;

        start_time = steady_clock::now();

        init_timers();
        trd = std::thread{[this]() { this->schedule(); }};

        running = true;
        return true;
    }

    /**
     * @brief 停止定时器调度器，之后可调用start()重新启动
     * @return 如果定时器调度器未运行，返回false
     */
    bool stop() {
        {
            std::lock_guard<std::mutex> guard{mtx};
            if (!running) return false;

            running = false;  // 必须放在临界区中
            cv.notify_one();  // 可以放在临界区中
        }
        trd.join();           // 不能放在临界区中
        return true;
    }

    nanoseconds operator()() const {
        std::lock_guard<std::mutex> guard{mtx};
        if (!running) {
            return nanoseconds::zero();
        }
        auto now{steady_clock::now()};
        auto elapsed{now - start_time};
        return elapsed;
    }

    void operator()(std::ostream& os) const {
        auto elapsed_time{this->operator()()};
        os << "TimerScheduler has been running for " << std::chrono::duration<double>{elapsed_time}.count() << "seconds"
           << std::endl;
    }

private:
    virtual size_t add_timer(std::function<void()> cb, microseconds delay, microseconds interval) = 0;

public:
    /**
     * @brief 创建重复触发的定时器，可以在调用start()之前和之后调用该函数
     * @param cb 定时器触发后执行的回调函数
     * @param delay 经过delay毫秒后第一次触发
     * @param interval 此后每隔interval毫秒触发一次
     * @return 新创建定时器的id
     */
    size_t submit(std::function<void()> cb, microseconds delay = microseconds::zero(),
                  microseconds interval = microseconds::zero()) {
        if (!cb) throw std::invalid_argument{"HeapTimerScheduler: timer callback is null."};
        if (delay < microseconds::zero() || interval < microseconds::zero())
            throw std::invalid_argument{"HeapTimerScheduler: delay or interval duration must greater than zero"};

        return add_timer(std::move(cb), delay, interval);
    }

    /**
     * @brief 取消id为timer_id的定时器
     * @param timer_id
     * @param wait 阻塞等待定时器调度器取消指定的定时器
     * @return 是否存在欲取消的定时器
     */
    virtual bool cancel(size_t timer_id, bool wait) = 0;
};

}  // namespace hipe
