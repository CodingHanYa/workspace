#pragma once
#include "header.h"

namespace hipe {

// thread object that support double queue replacement algorithm
class DqThread : public ThreadBase {
    std::queue<HipeTask> public_tq;
    std::queue<HipeTask> buffer_tq;
    util::spinlock tq_locker = {};

public:
    void runTasks() {
        while (!buffer_tq.empty()) {
            util::invoke(buffer_tq.front());
            buffer_tq.pop();
            task_numb--;
        }
    }

    bool tryLoadTasks() {
        tq_locker.lock();
        public_tq.swap(buffer_tq);
        tq_locker.unlock();
        return !buffer_tq.empty();
    }

    bool tryGiveTasks(DqThread& t) {
        if (tq_locker.try_lock()) {
            if (!public_tq.empty()) {
                auto numb = public_tq.size();
                public_tq.swap(t.buffer_tq);
                tq_locker.unlock();
                task_numb -= static_cast<int>(numb);
                t.task_numb += static_cast<int>(numb);
                return true;

            } else {
                tq_locker.unlock();
                return false;
            }
        }
        return false;
    }

    template <typename T>
    void enqueue(T&& tar) {
        util::spinlock_guard lock(tq_locker);
        public_tq.emplace(std::forward<T>(tar));
        task_numb++;
    }

    template <typename Container_>
    void enqueue(Container_& cont, size_t size) {
        util::spinlock_guard lock(tq_locker);
        for (size_t i = 0; i < size; ++i) {
            public_tq.emplace(std::move(cont[i]));
            task_numb++;
        }
    }
};

/**
 * @brief A steady thread pond.
 * Support task stealing and execute tasks in batches
 */
class SteadyThreadPond : public FixedThreadPond<DqThread> {
public:
    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
     */
    explicit SteadyThreadPond(int thread_numb = 0, int task_capacity = HipeUnlimited)
      : FixedThreadPond(thread_numb, task_capacity) {
        // create threads
        threads.reset(new DqThread[this->thread_numb]);
        for (int i = 0; i < this->thread_numb; ++i) {
            threads[i].bindHandle(std::thread(&SteadyThreadPond::worker, this, i));
        }
    }

    ~SteadyThreadPond() override = default;

private:
    void worker(int index) {
        auto& self = threads[index];

        while (!stop) {
            // yeild if no tasks
            if (self.notask()) {
                // notify the main thread
                if (self.isWaiting()) {
                    self.notifyTaskDone();
                    std::this_thread::yield();
                    continue;
                }
                // steal tasks from other threads
                if (enable_steal_tasks) {
                    for (int i = index, j = 0; j < max_steal; j++) {
                        util::recyclePlus(i, 0, thread_numb);
                        if (threads[i].tryGiveTasks(self)) {
                            self.runTasks();
                            break;
                        }
                    }
                    if (!self.notask() || self.isWaiting()) {
                        // go to handle the tasks or the waiting signal directly
                        continue;
                    }
                }
                std::this_thread::yield();

            } else {
                // run tasks
                if (self.tryLoadTasks()) {
                    self.runTasks();
                }
            }
        }
    }
};

}  // namespace hipe