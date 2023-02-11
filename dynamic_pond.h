#pragma once
#include "header.h"


namespace hipe {


/**
 * @brief A dynamic thread pond
 */
class DynamicThreadPond
{

    // stop the pond
    bool stop = {false};

    // the number of running threads
    std::atomic_int running_tnumb = {0};

    // expect running thread number
    std::atomic_int expect_tnumb = {0};

    // waiting for task done
    bool is_waiting_for_task = {false};

    // waiting for threads deleted
    bool is_waiting_for_thread = {false};

    // task number
    std::atomic_int total_tasks = {0};

    // shared task queue
    std::queue<HipeTask> shared_tq = {};

    // locker shared by threads
    std::mutex shared_locker;

    // cv to awake the paused thread
    std::condition_variable awake_cv = {};

    // task done
    std::condition_variable task_done_cv = {};

    // thread started or deleted
    std::condition_variable thread_cv = {};

    // dynamic thread pond
    std::list<std::thread> pond;

    // keep dead threads
    std::queue<std::thread> dead_threads;

    // the shrinking number of threads
    std::atomic_int shrink_numb = {0};

    // number of the tasks loaded by thread
    std::atomic_int tasks_loaded = {0};


public:
    /**
     * @brief construct DynamicThreadPond
     * @param tnumb initial thread number
     */
    explicit DynamicThreadPond(int tnumb = 0) {
        addThreads(tnumb);
    }

    ~DynamicThreadPond() {
        if (!stop) {
            close();
        }
    }

public:
    /**
     * @brief close the pond
     * Tasks blocking in the queue will be thrown.
     */
    void close() {
        stop = true;
        adjustThreads(0);
        waitForThreads();
        joinDeadThreads();
    }

    /**
     * @brief add threads
     * @param tnumb thread number
     * The pond will expand through creating new thread.
     */
    void addThreads(int tnumb = 1) {
        assert(tnumb >= 0);
        expect_tnumb += tnumb;
        HipeLockGuard lock(shared_locker);
        while (tnumb--) {
            pond.emplace_back(&DynamicThreadPond::worker, this, pond.rbegin());
        }
    }


    /**
     * @brief delete some threads
     * @param tnumb thread number
     * If there are not enough threads, the program will be interrupted.
     * The deletion will not happen immediately, but just notify that there are some threads 
     * need to be deleted, as a result, it is nonblocking.
     */
    void delThreads(int tnumb = 1) {
        assert((tnumb <= expect_tnumb) && (tnumb >= 0));
        expect_tnumb -= tnumb;
        shrink_numb += tnumb;
        HipeLockGuard lock(shared_locker);
        awake_cv.notify_all();
    }

    /**
     * @brief adjust thread number to target
     * @param target_tnumb target thread number
     */
    void adjustThreads(int target_tnumb) {
        assert(target_tnumb >= 0);
        if (target_tnumb > expect_tnumb) {
            addThreads(target_tnumb - expect_tnumb);
            return;
        }
        if (target_tnumb < expect_tnumb) {
            delThreads(expect_tnumb - target_tnumb);
            return;
        }
    }

    // join dead threads to recycle thread resource
    void joinDeadThreads() {
        while (!dead_threads.empty()) {
            shared_locker.lock();
            auto t = std::move(dead_threads.front());
            dead_threads.pop();
            shared_locker.unlock();
            t.join();
        }
    }


    // get task number of the pond, tasks in progress are also counted.
    int getTasksRemain() {
        return total_tasks.load();
    }

    // get number of the tasks loaded by thread
    int getTasksLoaded() {
        return tasks_loaded.load();
    }

    /**
     * reset the number of tasks loaded by thread and return the old value (atomic operation)
     * @return the old value
     */
    int resetTasksLoaded() {
        return tasks_loaded.exchange(0);
    }

    // get the number of running threads now
    int getRunningThreadNumb() const {
        return running_tnumb.load();
    }

    // get the number of expective running thread
    int getExpectThreadNumb() const {
        return expect_tnumb.load();
    }


    // wait for threads adjust
    void waitForThreads() {
        is_waiting_for_thread = true;
        HipeUniqGuard locker(shared_locker);
        thread_cv.wait(locker, [this] { return expect_tnumb == running_tnumb; });
        is_waiting_for_thread = false;
    }


    // wait for tasks in the pond done
    void waitForTasks() {
        is_waiting_for_task = true;
        HipeUniqGuard locker(shared_locker);
        task_done_cv.wait(locker, [this] { return !total_tasks; });
        is_waiting_for_task = false;
    }

    /**
     * @brief submit task
     * @param foo An runnable object
     */
    template <typename Runnable>
    void submit(Runnable&& foo) {
        {
            HipeLockGuard lock(shared_locker);
            shared_tq.emplace(std::forward<Runnable>(foo));
            ++total_tasks;
        }
        awake_cv.notify_one();
    }

    /**
     * @brief submit task and get return
     * @param foo a runnable object
     * @return a future
     */
    template <typename Runnable>
    auto submitForReturn(Runnable&& foo) -> std::future<typename std::result_of<Runnable()>::type> {
        using RT = typename std::result_of<Runnable()>::type;
        std::packaged_task<RT()> pack(std::forward<Runnable>(foo));
        std::future<RT> fut(pack.get_future());
        {
            HipeLockGuard lock(shared_locker);
            shared_tq.emplace(std::move(pack));
            ++total_tasks;
        }
        awake_cv.notify_one();
        return fut;
    }

    /**
     * submit in a batch and the task container must override "[]"
     * @param cont task container
     * @param size the size of the container
     */
    template <typename Container_>
    void submitInBatch(Container_& cont, size_t size) {
        {
            HipeLockGuard lock(shared_locker);
            total_tasks += static_cast<int>(size);
            for (size_t i = 0; i < size; ++i) {
                shared_tq.emplace(std::move(cont[i]));
            }
        }
        awake_cv.notify_all();
    }


private:
    using Iter = std::list<std::thread>::reverse_iterator;

    void notifyThreadAdjust() {
        HipeLockGuard lock(shared_locker);
        thread_cv.notify_one();
    }

    // working threads' default loop
    void worker(Iter it) {
        // task container
        HipeTask task;

        running_tnumb++;
        if (is_waiting_for_thread) {
            notifyThreadAdjust();
        }

        do {
            HipeUniqGuard locker(shared_locker);
            awake_cv.wait(locker, [this] { return !shared_tq.empty() || shrink_numb > 0; });

            // receive deletion inform
            if (shrink_numb) {
                shrink_numb--;
                dead_threads.emplace(std::move(*it)); // save std::thread
                pond.erase((++it).base());
                break;
            }
            task = std::move(shared_tq.front());
            shared_tq.pop();
            locker.unlock();

            tasks_loaded++;

            util::invoke(task);
            --total_tasks;

            if (is_waiting_for_task) {
                HipeLockGuard lock(shared_locker);
                task_done_cv.notify_one();
            }

        } while (true);

        running_tnumb--;
        if (is_waiting_for_thread) {
            notifyThreadAdjust();
        }
    }
};


} // namespace hipe