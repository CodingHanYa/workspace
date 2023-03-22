#pragma once
#include "header.h"

namespace hipe {

class OqThread : public ThreadBase {
    HipeTask task;
    std::queue<HipeTask> tq;
    // util::spinlock tq_locker = {};
    std::mutex tq_locker;

public:
    /**
     * @brief try give one task to another thread
     * @param other another thread
     * @return if succeed —— return true, or return false
     */
    bool tryGiveTask(OqThread& another) {
        if (tq_locker.try_lock()) {
            if (!tq.empty()) {
                another.task = std::move(tq.front());
                tq.pop();
                tq_locker.unlock();
                this->task_numb--;
                another.task_numb++;
                return true;
            } else {
                tq_locker.unlock();
                return false;
            }
        }
        return false;
    }

    // push task to the task queue
    template <typename T>
    void enqueue(T&& tar) {
        tq_locker.lock();
        tq.emplace(std::forward<T>(tar));
        task_numb++;
        tq_locker.unlock();
    }

    // push tasks to the task queue
    template <typename Container_>
    void enqueue(Container_& cont, size_t size) {
        tq_locker.lock();
        for (size_t i = 0; i < size; ++i) {
            tq.emplace(std::move(cont[i]));
            task_numb++;
        }
        tq_locker.unlock();
    }

    // run the task
    void runTask() {
        util::invoke(task);
        task_numb--;
    }

    // try load task from the task queue
    bool tryLoadTask() {
        tq_locker.lock();
        if (!tq.empty()) {
            task = std::move(tq.front());
            tq.pop();
            tq_locker.unlock();
            return true;
        } else {
            tq_locker.unlock();
            return false;
        }
    }
};

class BalancedThreadPond : public FixedThreadPond<OqThread> {
public:
    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
     */
    explicit BalancedThreadPond(int thread_numb = 0, int task_capacity = HipeUnlimited)
      : FixedThreadPond(thread_numb, task_capacity) {
        // create
        threads.reset(new OqThread[this->thread_numb]);

        for (int i = 0; i < this->thread_numb; ++i) {
            threads[i].bindHandle(std::thread(&BalancedThreadPond::worker, this, i));
        }
    }
    ~BalancedThreadPond() override = default;

private:
    void worker(int index) {
        auto& self = threads[index];

        while (!stop) {
            // yield if no tasks
            if (self.notask()) {
                if (self.isWaiting()) {
                    self.notifyTaskDone();
                    std::this_thread::yield();
                    continue;
                }

                // steal tasks from other threads
                if (enable_steal_tasks) {
                    for (int i = index, j = 0; j < max_steal; j++) {
                        util::recyclePlus(i, 0, thread_numb);
                        if (threads[i].tryGiveTask(self)) {
                            self.runTask();
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
                // try load task and run
                if (self.tryLoadTask()) {
                    self.runTask();
                }
            }
        }
    }
};

}  // namespace hipe
