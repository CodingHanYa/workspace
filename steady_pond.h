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

        std::atomic_int task_numb = {0};
        std::condition_variable task_done_cv;


        /**
         * @brief default loop of async thread
         * @param index position of the thread
         * @param pond provide some resource like locker
        */
        void worker(int index, SteadyThreadPond* pond) 
        {
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
            auto robNeighbor = [this, index, pond] 
            {
                int tmp = index;
                for (int i = 0; i < pond->rob_numb; ++i) 
                {
                    util::recyclePlus(tmp, 0, pond->thread_numb);
                    if (pond->tq_locker.try_lock()) 
                    {
                        auto thr = &(pond->threads[tmp]);
                        auto que = &thr->public_tq;

                        if (que->size()) 
                        {
                            thr->task_numb -= que->size();
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
                    std::this_thread::yield();
                    continue;
                }
                if (!pond->stop) 
                {
                    loadTask();
                    runBufferTaskQueue();

                    if (pond->enable_rob_tasks && robNeighbor()) 
                        runBufferTaskQueue();

                    // main thread waiting for working thread
                    if (pond->waiting)
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
    bool stop = {false};     

    // wait for tasks done
    bool waiting = {false};

    // enable rob tasks from neighbor thread
    bool enable_rob_tasks = true;

    // max rob numb
    uint rob_numb = 2;

    // iterater for threads
    int cur = 0;                              

    // thread number
    int thread_numb = 0;      

    // step limitation of seeking the least-busy thread
    int step_limitation = 4;

    // task capacity per thread
    uint thread_cap = 0;

    // task queue locker for threads
    util::spinlock tq_locker;
    
    // condition variable locker for threads
    std::mutex cv_locker;

    // keep thread variables
    std::unique_ptr<Thread[]> threads = {nullptr};    

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
     * If the capacity is unlimited , the hipe will throw logic error. 
     * If didn't set and task overflow, the hipe will throw logic error and abort the program. 
    */
    template <typename F, typename... _Argv>
    void setRefuseCallBack(F&& foo, _Argv&&... argv) 
    {
        if (!thread_cap) {
            throw std::logic_error("The refuse callback will never be invoked because the capacity has been setted unlimited");
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
    util::Block<HipeTask>  pullOverFlowTasks() {
        auto tmp = std::move(overflow_tasks);
        return tmp;
    }

    /**
     * Wait for all threads finish their task
    */
    void waitForTasks() 
    {
        waiting = true;
        for (int i = 0; i < thread_numb; ++i) 
        {
            HipeUniqGuard lock(cv_locker);
            threads[i].task_done_cv.wait(lock, [this, i]{ 
                return !threads[i].task_numb; 
            });
        }
        waiting = false;
    }

    /**
     * @brief submit task
     * @param foo a runable object
    */
    template <typename _Runable>
    void submit(_Runable&& foo) 
    {
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
     * @brief submit task and get return
     * @param foo a runable object
     * @return a future 
    */
    template <typename _Runable>
    auto submitForReturn(_Runable&& foo) -> std::future<typename std::result_of<_Runable()>::type> 
    {
        if (thread_cap && !admit()) {
            taskOverFlow(std::forward<_Runable>(foo));
        }
        moveIndexForLeastBusy(cur);

        using RT = typename std::result_of<_Runable()>::type;
        std::packaged_task<RT()> pack(std::move(foo));
        std::future<RT> fut(pack.get_future()); 

        util::spinlock_guard lock(tq_locker);
        threads[cur].public_tq.emplace(std::move(pack));
        threads[cur].task_numb++;

        return fut; 
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
            int start = cur;
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


    // enable task stealing between each thread
    void enableRobTasks() {
        enable_rob_tasks = true;
    }

    // disable task stealing between each thread
    void disableRobTasks() {
        enable_rob_tasks = false;
    }

    // set thread number of each rob 
    void setRobThreadNumb(uint numb) {
        if (!numb || numb >= thread_numb) {
            throw std::invalid_argument("The number of robbing threads must smaller than thread number and greater than zero");
        }
        rob_numb = numb;
    }



private:

    /**
     * Judge whether there are enough capacity for tasks, 
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
        overflow_tasks.reset(1);
        overflow_tasks.add(std::forward<T>(task));

        if (refuse_cb.is_setted()) {
            util::invoke(refuse_cb);
        } 
        else {
            throw std::runtime_error("SteadyThreadPond: Task overflow while submitting task");
        }
    }

    /**
     * task overflow call back for batch submit
     * @param left left edge
     * @param right right edge
    */ 
    template <typename T>
    void taskOverFlow(T&& tasks, size_t left, size_t right) 
    {
        overflow_tasks.reset(right-left);

        for (int i = left; i < right; ++i) {
            overflow_tasks.add(std::move(tasks[i]));
        }
        if (refuse_cb.is_setted()) {
            util::invoke(refuse_cb);
        } 
        else {
            throw std::runtime_error("SteadyThreadPond: Task overflow while submitting tasks in batch");
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