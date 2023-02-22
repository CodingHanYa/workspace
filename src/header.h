#pragma once
#include "./util.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace hipe {

// ======================
//        alias
// ======================

static const int HipeUnlimited = 0;

/**
 * @brief task type that is able to contain different kinds of executable object!
 * It is quite useful.
 */
using HipeTask = util::SafeTask;

using HipeLockGuard = std::lock_guard<std::mutex>;

using HipeUniqGuard = std::unique_lock<std::mutex>;

using HipeTimePoint = std::chrono::steady_clock::time_point;

template <typename T>
using HipeFutures = util::Futures<T>;


class ThreadPoolError : public std::exception
{
private:
    std::string message;

public:
    explicit ThreadPoolError(std::string msg)
      : message{std::move(msg)} {
    }
    const char* what() const noexcept override {
        return message.data();
    }
};

class TaskOverflowError : public ThreadPoolError
{
};

// A thread that can automatically joined.
class AutoThread
{
    using id = std::thread::id;
    using native_handle_type = std::thread::native_handle_type;

private:
    std::thread thread_;

public:
    AutoThread() noexcept = default;

    template <typename Callable, typename... Args>
    explicit AutoThread(Callable&& func, Args&&... args)
      : thread_{std::forward<Callable>(func), std::forward<Args>(args)...} {
    }

    AutoThread(const AutoThread&) = delete;
    AutoThread& operator=(const AutoThread&) = delete;
    AutoThread(AutoThread&&) noexcept = default;

    AutoThread& operator=(AutoThread&& other) noexcept {
        AutoThread(std::move(other)).swap(*this);
        return *this;
    }

    ~AutoThread() {
        if (joinable()) {
            join();
        }
    }

    void swap(AutoThread& other) noexcept {
        std::swap(thread_, other.thread_);
    }

    friend void swap(AutoThread& lhs, AutoThread& rhs) noexcept {
        lhs.swap(rhs);
    }

    bool joinable() const noexcept {
        return thread_.joinable();
    }

    void join() {
        thread_.join();
    }

    void detach() {
        thread_.detach();
    }

    id get_id() const noexcept {
        return thread_.get_id();
    }

    native_handle_type native_handle() {
        return thread_.native_handle();
    }

    static unsigned hardware_concurrency() noexcept {
        return std::thread::hardware_concurrency();
    }
};

class ThreadBase
{
protected:
    bool waiting = false;
    AutoThread handle;

    std::atomic_int task_numb = {0};
    std::condition_variable task_done_cv;
    std::mutex cv_locker;

public:
    ThreadBase() = default;
    virtual ~ThreadBase() = default;

    int getTasksNumb() {
        return task_numb.load();
    }

    bool notask() {
        return !task_numb;
    }

    void join() {
        handle.join();
    }

    void bindHandle(AutoThread&& handle_) {
        this->handle = std::move(handle_);
    }

    bool isWaiting() const {
        return waiting;
    }

    void waitTasksDone() {
        waiting = true;
        HipeUniqGuard lock(cv_locker);
        task_done_cv.wait(lock, [this] { return !task_numb; });
    }

    void cleanWaitingFlag() {
        waiting = false;
    }

    void notifyTaskDone() {
        HipeUniqGuard lock(cv_locker);
        task_done_cv.notify_one();
    }
};


/**
 * @brief Basic class of thread pond that has defined all mechanism except async thread's loop.
 * @tparam The type of thread wrapper class that inherited from ThreadBase.
 */
template <typename Ttype>
class FixedThreadPond
{
protected:
    // stop the thread pend
    bool stop = {false};

    // thread number
    int thread_numb = 0;

    // cursor of the thread pond, used to travel the pond
    int cursor = 0;

    // cursor's move limit
    int cursor_move_limit = 0;

    // max steal thread number
    int max_steal = 0;

    // whether enable tasks stealing
    bool enable_steal_tasks = false;

    // threads
    std::unique_ptr<Ttype[]> threads = {nullptr};

    // task capacity per thread
    int thread_cap = 0;

    // tasks that failed to submit
    util::Block<HipeTask> overflow_tasks{0};

    // task overflow call back
    HipeTask refuse_cb;

protected:
    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
     * @param type_limit  Use SFINAE to restrict the type of template parameter only to be inherited from ThreadBase
     */
    explicit FixedThreadPond(
        int thread_numb = 0, int task_capacity = HipeUnlimited,
        typename std::enable_if<std::is_base_of<ThreadBase, Ttype>::value>::type* type_limit = nullptr) {

        assert(thread_numb >= 0);
        assert(task_capacity >= 0);

        // calculate thread number
        if (!thread_numb) {
            int tmp = static_cast<int>(std::thread::hardware_concurrency());
            this->thread_numb = (tmp > 0) ? tmp : 1;
        } else {
            this->thread_numb = thread_numb;
        }

        // calculate task capacity
        if (!task_capacity) {
            thread_cap = 0;
        } else if (task_capacity > this->thread_numb) {
            this->thread_cap = task_capacity / this->thread_numb;
        } else {
            this->thread_cap = 1;
        }

        // load balance
        cursor_move_limit = getBestMoveLimit(thread_numb);
    }

    virtual ~FixedThreadPond() {
        if (!stop) {
            close();
        }
    }

public:
    // ====================================================
    //                Universal interfaces
    // ====================================================


    /**
     * Wait until all threads finish their task
     */
    void waitForTasks() {
        for (int i = 0; i < thread_numb; ++i) {
            threads[i].waitTasksDone();
        }
        for (int i = 0; i < thread_numb; ++i) {
            threads[i].cleanWaitingFlag();
        }
    }


    /**
     * @brief Close the pond.
     * Notice that the tasks that are still waiting will never been executed.
     * If you want to make sure that all tasks executed, call waitForTasks() before close.
     */
    void close() {
        stop = true;
        for (int i = 0; i < thread_numb; ++i) {
            threads[i].join();
        }
    }

    /**
     * get block tasks number now
     */
    int getTasksRemain() {
        int ret = 0;
        for (int i = 0; i < thread_numb; ++i) {
            ret += threads[i].getTasksNumb();
        }
        return ret;
    }


    /**
     * get the number of threads
     */
    int getThreadNumb() {
        return thread_numb;
    }

    /**
     * @brief submit task
     * @param foo a runable object
     */
    template <typename F>
    void submit(F&& foo) {
        if (!admit()) {
            taskOverFlow(std::forward<F>(foo));
            return;
        }
        auto t = getLeastBusyThread();
        t->enqueue(std::forward<F>(foo));
    }

    /**
     * @brief submit task and get return
     * @param foo a runable object
     * @return a future
     */
    template <typename F>
    auto submitForReturn(F&& foo) -> std::future<typename std::result_of<F()>::type> {
        if (!admit()) {
            taskOverFlow(std::forward<F>(foo));
            return std::future<typename std::result_of<F()>::type>();
        }

        using RT = typename std::result_of<F()>::type;
        std::packaged_task<RT()> pack(std::forward<F>(foo));
        std::future<RT> fut(pack.get_future());

        auto t = getLeastBusyThread();
        t->enqueue(std::move(pack));
        return fut;
    }


    /**
     * submit in a batch and the task container must override "[]"
     * @param cont tasks container
     * @param size the size of the container
     */
    template <typename Container_>
    void submitInBatch(Container_&& container, size_t size) {
        if (thread_cap) {
            moveCursorToLeastBusy();
            for (size_t i = 0; i < size; ++i) {
                // admit one task
                if (admit()) {
                    getThreadNow()->enqueue(std::move(container[i]));
                } else {
                    taskOverFlow(std::forward<Container_>(container), i, size);
                    break;
                }
            }
        } else {
            getLeastBusyThread()->enqueue(std::forward<Container_>(container), size);
        }
    }


protected:
    // ====================================================
    //              load balancing mechanism
    // ====================================================


    /**
     * Move cursor to the least busy thread and then get
     * pointer of it.
     */
    Ttype* getLeastBusyThread() {
        moveCursorToLeastBusy();
        return &threads[cursor];
    }

    /**
     * Move cursor to the least busy thread.
     * If the thread that pointed by the cursor has been the least busy one then the cursor will not move.
     */
    void moveCursorToLeastBusy() {
        int tmp = cursor;
        for (int i = 0; i < cursor_move_limit; ++i) {
            if (threads[cursor].getTasksNumb()) {
                cursor = (threads[tmp].getTasksNumb() < threads[cursor].getTasksNumb()) ? tmp : cursor;
                util::recyclePlus(tmp, 0, thread_numb);
            } else {
                break;
            }
        }
    }

    // calculate best cursor move limit
    int getBestMoveLimit(int thread_number) {
        if (thread_number == 1) {
            return 0;
        }
        int tmp = thread_number / 4;
        tmp = (tmp < 1) ? 1 : tmp;
        return (tmp > 4) ? 4 : tmp;
    }

public:
    // enable task stealing between threads
    void enableStealTasks(int max_numb = 0) {
        assert(max_numb >= 0);

        if (!max_numb) {
            max_numb = std::max(thread_numb / 4, 1);
            max_numb = std::min(max_numb, 8);
        }
        if (max_numb >= thread_numb) {
            throw std::invalid_argument(
                "[HipeError]: The number of stealing threads must smaller than thread number and greater than zero");
        }
        max_steal = max_numb;
        enable_steal_tasks = true;
    }

    // disable task stealing between each thread
    void disableStealTasks() {
        enable_steal_tasks = false;
    }


public:
    // ====================================================
    //               task overflow mechanism
    // ====================================================


    /**
     * Set refuse call back.
     * If the capacity is unlimited , the hipe will throw a logic error.
     * If didn't set and refuse call back, the hipe will throw logic error and abort the program.
     */
    template <typename F, typename... Args>
    void setRefuseCallBack(F&& foo, Args&&... args) {
        static_assert(util::is_runnable<F, Args...>::value, "[HipeError]: The refuse callback is a non-runnable object");
        if (!thread_cap) {
            throw std::logic_error(
                "[HipeError]: The refuse callback will never be invoked because the capacity has been set unlimited");
        } else {
            refuse_cb.reset(std::bind(std::forward<F>(foo), std::forward<Args>(args)...));
        }
    }

    /**
     * Pull the overflow tasks managed by a block which will be kept until next task overflow.
     * Then the new tasks will replace the old.
     */
    util::Block<HipeTask> pullOverFlowTasks() {
        auto tmp = std::move(overflow_tasks);
        return tmp;
    }

protected:
    Ttype* getThreadNow() {
        return &threads[cursor];
    }


    /**
     * Judge whether there are enough capacity for tasks,
     * if the task capacity of the pond is unlimited, it will always return true.
     * This function will possibly move the cursor of the thread pond for enough capacity.
     * @param tar_capacity target capacity
     */
    bool admit(int tar_capacity = 1) {
        if (!thread_cap) {
            return true;
        }
        int prev = cursor;
        auto spare = [this, tar_capacity](Ttype& t) { return (t.getTasksNumb() + tar_capacity) <= thread_cap; };
        while (!spare(threads[cursor])) {
            util::recyclePlus(cursor, 0, thread_numb);
            if (cursor == prev)
                return false;
        }
        return true;
    }


    // task overflow callback for one task
    template <typename T>
    void taskOverFlow(T&& task) {
        overflow_tasks.reset(1);
        overflow_tasks.add(std::forward<T>(task));

        if (refuse_cb.is_set()) {
            util::invoke(refuse_cb);
        } else {
            throw std::runtime_error("[HipeError]: Task overflow while submitting task");
        }
    }


    /**
     * task overflow callback for batch submit
     * @param left left edge(included)
     * @param right right edge(not included)
     */
    template <typename T>
    void taskOverFlow(T&& tasks, int left, int right) {
        overflow_tasks.reset(right - left);

        for (int i = left; i < right; ++i) {
            overflow_tasks.add(std::move(tasks[i]));
        }
        if (refuse_cb.is_set()) {
            util::invoke(refuse_cb);
        } else {
            throw std::runtime_error("[HipeError]: Task overflow while submitting tasks in batch");
        }
    }
};

} // namespace hipe