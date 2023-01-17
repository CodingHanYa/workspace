#pragma once
#include "header.h"

namespace hipe { 

/**
 * @brief A steady thread pond. 
 * @tparam Type that is executable.
 * Thread object inside support <[ Double queue replacement algorithm ]>
*/

class SteadyThreadPond
{

private:
    // thread object that support double queue replacement algorithm
    struct Thread
    {
        std::thread handle;

        std::queue<HipeTask> public_tq;
        std::queue<HipeTask> buffer_tq;

        std::atomic_bool waiting = {false};
        std::atomic_int  task_numb = {0};

        std::condition_variable awake_cv;
        std::condition_variable task_done_cv;


        /**
         * @brief default loop of async thread
         * @param self_idx position of the thread
         * @param pond using to call source from the pond
        */
        void worker(int self_idx, SteadyThreadPond* pond) 
        {
            
            // wait for tasks
            auto waitNotify = [this, pond] {
                HipeUniqGuard lock(pond->cv_locker);
                awake_cv.wait(lock, [this, pond]{return (task_numb.load() > 0 || pond->stop);});
            };

            // execute buffer task queue
            auto runBufferTaskQueue = [this, pond] 
            {
                auto sz = buffer_tq.size();
                while (!buffer_tq.empty()) {
                    util::invoke(buffer_tq.front());
                    buffer_tq.pop();
                }
                task_numb -= sz;
            };

            // try rob task from neighbor thread's public task queue
            // if succeed ——> return true.
            auto robNeighbor = [this, self_idx, pond] 
            {
                int idx = self_idx;
                for (int i = 0; i < HIPE_MAX_ROB_NEIG_STEP; ++i) 
                {
                    util::recyclePlus(idx, 0, pond->thread_numb);
                    if (pond->tq_locker.try_lock()) 
                    {
                        auto neig = &(pond->threads[idx]);
                        auto que = &neig->public_tq;

                        if (que->size()) 
                        {
                            neig->task_numb -= que->size();
                            que->swap(buffer_tq);
                            task_numb += buffer_tq.size();

                            pond->tq_locker.unlock();
                            return true;
                        }
                        pond->tq_locker.unlock();
                    }
                }
                return false;
            };

            // swap task queue for task
            auto loadTask = [this, pond] {
                util::spinlock_guard lock(pond->tq_locker);
                public_tq.swap(buffer_tq);
            };

            while (!pond->stop) 
            {
                if (!task_numb) {
                    if (waiting) task_done_cv.notify_one();
                    std::this_thread::yield();
                    continue;
                }
                if (!pond->stop) 
                {
                    loadTask();
                    runBufferTaskQueue();

                    // enable rob task
                    if (robNeighbor()) 
                        runBufferTaskQueue();

                    // main thread waiting for working thread
                    if (waiting) 
                    {
                        // clean the public task queue
                        loadTask();
                        runBufferTaskQueue();

                        // tell main thread that tasks done
                        task_done_cv.notify_one();
                    }

                }
            }
        }

    };

private:

    // stop the thread pend
    std::atomic_bool stop = {false};     

    // iterater for threads
    int cur = 0;                              

    // thread number
    int thread_numb = 0;      

    // step limitation of seeking the least-busy thread
    int step_limitation = 4;

    // capacity per thread
    uint thread_cap = 0;

    // task queue locker for threads
    util::spinlock tq_locker;
    
    // condition variable locker for threads
    std::mutex cv_locker;

    // keep thread variables
    std::unique_ptr<Thread[]> threads = {nullptr};    

    // overflow tasks' locker
    std::mutex of_tasks_locker;

    // tasks that failed to submit
    util::Block<HipeTask> overflow_tasks = {0};

    // task overflow call back
    HipeTask refuse_cb;

    // grant permission
    friend struct Thread;                      

public:

    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
    */
    SteadyThreadPond(uint thread_numb = 0, uint task_capacity = HipeUnlimited) 
    {
        if (!thread_numb) {
            uint tmp = std::thread::hardware_concurrency();
            this->thread_numb = (tmp > 0) ? tmp : 1; 
        } else {
            this->thread_numb = thread_numb;
        }

        if (!task_capacity) {
            thread_cap = 0;
        } else if (task_capacity > this->thread_numb) {
            this->thread_cap = task_capacity/this->thread_numb;
        } else {
            this->thread_cap = 1;
        }

        this->step_limitation = getBestMoveStep(thread_numb);
        
        threads.reset(new Thread[this->thread_numb]);

        for (int i = 0; i < this->thread_numb; ++i) {
            threads[i].handle = std::thread(&Thread::worker, &threads[i], i, this);
        }
    }
    
    ~SteadyThreadPond() {
        if (!stop) close(); 
    }

public:

    /**
     * Set refuse call back. 
     * If the capacity is unlimited , the hipe will throw error. 
     * If didn't set and task overflow, the hipe will throw error and abort the program. 
    */
    template <typename F, typename... _Argv>
    void setRefuseCallBack(F&& foo, _Argv&&... argv) 
    {
        if (!thread_cap) {
            util::error("The refuse callback will never be invoked because the capacity has been setted unlimited");
        } else {
            refuse_cb.reset(std::bind(std::forward<F>(foo), std::forward<_Argv>(argv)...));
        }
    }

    /**
     * @brief Close the pond. 
     * Notice that the tasks that are still waiting will never been executed.
     * If you want to make sure that all tasks executed, call waitForTasks() before close. 
    */ 
    void close() 
    {
        stop = true;
        HipeUniqGuard locker(cv_locker);
        for (int i = 0; i < thread_numb; ++i)  { 
            threads[i].awake_cv.notify_one();
        }
        locker.unlock();
        
        for (int i = 0; i < thread_numb; ++i) {
            threads[i].handle.join();
        }
    }

    
    /**
     * get block tasks number now
    */
    int getTasksRemain() 
    {
        int ret = 0;
        for (int i = 0; i < thread_numb; ++i) {
            ret += threads[i].task_numb.load();
        }
        return ret;
    }              

    /**
     * Pull the overflow tasks managed by a block which will be kept until next task overflow. 
     * Then the new tasks will replace the old.
    */
    util::Block<HipeTask>&&  pullOverFlowTasks() {
        HipeLockGuard locker(of_tasks_locker);
        return std::move(overflow_tasks);
    }

    /**
     * Wait for all threads finish their task
    */
    void waitForTasks() 
    {
        for (int i = 0; i < thread_numb; ++i) 
        {
            threads[i].waiting = true;

            HipeUniqGuard lock(cv_locker);
            threads[i].task_done_cv.wait(lock, [this, i]{ 
                return !threads[i].task_numb; 
            });
            threads[i].waiting = false;
        }
    }

    /**
     * @brief submit task
     * @param foo An runable object
    */
    template <typename _Runable>
    void submit(_Runable&& foo) 
    {
        // if the least-busy thread can't admit the task
        if (thread_cap && !admit()) {
            taskOverFlow(std::forward<_Runable>(foo));
            return;
        }
        moveIndexForLeastBusy(cur);

        util::spinlock_guard lock(tq_locker);
        threads[cur].public_tq.emplace(std::forward<_Runable>(foo));
        threads[cur].task_numb++;
    }

    /**
     * @brief submit task
     * @param foo An runable object
     * @param times The times you want to execute
    */
    template <typename _Runable>
    void submit(_Runable&& foo, uint times) 
    {
        if (thread_cap && !admit(times)) {
            taskOverFlow(std::forward<_Runable>(foo));
            return;
        }
        moveIndexForLeastBusy(cur);

        util::spinlock_guard lock(tq_locker);
        for (int i = 0; i < times; ++i) {
            threads[cur].public_tq.emplace(std::forward<_Runable>(foo));
            threads[cur].task_numb++;
        }
    }

    /**
     * submit in a batch and the task container must override "[]"
     * @param cont tasks container
     * @param size the size of the container
    */
    template <typename _Container>
    void submitInBatch(_Container&& container, uint size) 
    {
        moveIndexForLeastBusy(cur);

        if (thread_cap) 
        {
            auto start = cur;
            for (int i = 0; i < size; ) 
            {
                if (threads[cur].task_numb.load() < thread_cap) {
                    util::spinlock_guard lock(tq_locker);
                    threads[cur].public_tq.emplace(std::move(container[i++]));
                    threads[cur].task_numb++;
                } 
                else {
                    util::recyclePlus(cur, 0, thread_numb);
                    if (cur == start) {
                        taskOverFlow(std::forward<_Container>(container), i, size);
                        return;
                    }
                }
            }
        } else {

            util::spinlock_guard lock(tq_locker);
            for (uint i = 0; i < size; ) {
                threads[cur].public_tq.emplace(std::move(container[i++]));
                threads[cur].task_numb++;
            }
        }
    }

private:

    /**
     * Judge whether there are enough capacity, 
     * if the task capacity of the pond is unlimited, it will always return true
     * @param tar_capacity target capacity
    */
    bool admit(uint tar_capacity = 1) 
    {
        if (!thread_cap) {
            return true;
        }
        int prev = cur;
        auto spare = [this, tar_capacity] (Thread& t) {
            return (t.task_numb.load() + tar_capacity) <= thread_cap;
        };
        while (!spare(threads[cur])) 
        {
            util::recyclePlus(cur, 0, thread_numb);
            if (cur == prev) return false;
        }
        return true;
    }

    // task overflow call back for one task
    template <typename T>
    void taskOverFlow(T&& task) 
    {    
        {
            HipeLockGuard lock(of_tasks_locker);
            overflow_tasks.reset(1);
            overflow_tasks.add(std::forward<T>(task));
        }

        if (refuse_cb.is_setted()) {
            util::invoke(refuse_cb);
        } 
        else {
            util::error("SteadyThreadPond: Task overflow while submitting task");
        }
    }

    /**
     * task overflow call back for batch
     * @param left left edge
     * @param right right edge
    */ 
    template <typename T>
    void taskOverFlow(T&& tasks, size_t left, size_t right) 
    {
        {
            HipeLockGuard lock(of_tasks_locker);
            overflow_tasks.reset(right-left);

            for (int i = left; i < right; ++i) {
                overflow_tasks.add(std::move(tasks[i]));
            }
        }
        if (refuse_cb.is_setted()) {
            util::invoke(refuse_cb);
        } 
        else {
            util::error("SteadyThreadPond: Task overflow while submitting task by batch");
        }
    }

    // move index to least-busy thread
    void moveIndexForLeastBusy(int& idx) 
    {
        int tmp = idx;
        util::recyclePlus(tmp, 0, thread_numb);

        for (int i = 0; i < step_limitation; ++i) {
            if (threads[idx].task_numb) {
                idx = (threads[tmp].task_numb < threads[idx].task_numb) ? tmp : idx;
                util::recyclePlus(tmp, 0, thread_numb);
            } else {
                break;
            }
        }
    }

    // calculate best step for seeking least-busy thread 
    short getBestMoveStep(int thread_numb) 
    {
        short tmp = thread_numb / 4;
        tmp = (tmp < 1) ? 1 : tmp;
        return (tmp > step_limitation) ? step_limitation : tmp;
    }

};

}