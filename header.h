#pragma once
#include "./util.h"
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <future>
#include <atomic>
#include <memory>
#include <thread>
#include <iostream>
#include <functional>
#include <condition_variable>

namespace hipe {

// ======================
//        alias
// ======================

static const int HipeUnlimited = 0;

/**
 * @brief task type that is able to contain different kinds of executable object!
 * It is quite useful.
*/
using HipeTask = util::Task;

using HipeLockGuard = std::lock_guard<std::mutex>;

using HipeUniqGuard = std::unique_lock<std::mutex>;

using HipeTimePoint = std::chrono::steady_clock::time_point;

template <typename T> 
using HipeFutures  = util::Futures<T>;


class ThreadBase 
{
public:

    bool waiting = false;
    std::thread handle;

    std::atomic_int task_numb = {0};
    std::condition_variable task_done_cv;
    std::mutex cv_locker;

public:

    virtual uint getTasksNumb() {
        return task_numb.load();
    }

    virtual bool notask() {
        return !task_numb;
    }

    virtual void join() {
        handle.join();
    }

    virtual void bindHandle(std::thread&& handle) {
        this->handle = std::move(handle);
    }

    virtual void waitTasksDone() {
        waiting = true;
        HipeUniqGuard lock(cv_locker);
        task_done_cv.wait(lock, [this]{ return !task_numb; });
        waiting = false;
    }

};

}