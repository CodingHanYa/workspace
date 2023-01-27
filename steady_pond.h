#pragma once
#include "header.h"

namespace hipe { 

// thread object that support double queue replacement algorithm
class DqThread: public ThreadBase
{
    std::queue<HipeTask> public_tq;
    std::queue<HipeTask> buffer_tq;
    util::spinlock tq_locker = {};

public:

    void runTasks() 
    {   
        tq_locker.lock();
        public_tq.swap(buffer_tq);
        tq_locker.unlock();

        while (!buffer_tq.empty()) {
            util::invoke(buffer_tq.front());
            buffer_tq.pop();
            task_numb--;
        }
        if (waiting) {
            HipeUniqGuard lock(cv_locker);
            task_done_cv.notify_one();
        }
    }

    void runBufferTasks() 
    {
        while (!buffer_tq.empty()) {
            util::invoke(buffer_tq.front());
            buffer_tq.pop();
            task_numb--;
        }   
        if (waiting) {
            HipeUniqGuard lock(cv_locker);
            task_done_cv.notify_one();
        }
    }    

    bool tryGiveTasks(DqThread& t)
    {
        if (tq_locker.try_lock()) 
        {
            if (public_tq.size()) 
            {
                auto numb = public_tq.size(); 
                public_tq.swap(t.buffer_tq);
                task_numb -= numb;
                t.task_numb += numb;
                tq_locker.unlock();
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

    template <typename _Container>
    void enqueue(_Container& cont, uint size) {
        util::spinlock_guard lock(tq_locker);
        for (int i = 0; i < size; ++i) {
            public_tq.emplace(std::move(cont[i]));
            task_numb++;
        }
    }

};



/**
 * @brief A steady thread pond. 
 * @tparam Type that is executable.
 * Thread object inside support <[ Double queue replacement algorithm ]>
*/

class SteadyThreadPond
{
    // stop the thread pend
    bool stop = {false};     

    // enable steal tasks from neighbor thread
    bool enable_steal_tasks = false;

    // max steal numb
    uint max_steal = 0;

    // iterater for threads
    int cur = 0;                              

    // thread number
    int thread_numb = 0;      

    // step limitation of seeking the least-busy thread
    int step_limitation = 4;

    // task capacity per thread
    uint thread_cap = 0;

    // keep thread variables
    std::unique_ptr<DqThread[]> threads = {nullptr};    

    // tasks that failed to submit
    util::Block<HipeTask> overflow_tasks = {0};

    // task overflow call back
    HipeTask refuse_cb;

public:

    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
    */
    SteadyThreadPond(uint thread_numb = 0, uint task_capacity = HipeUnlimited) 
    {
        // calculate thread number
        if (!thread_numb) {
            uint tmp = std::thread::hardware_concurrency();
            this->thread_numb = (tmp > 0) ? tmp : 1; 
        } else {
            this->thread_numb = thread_numb;
        }

        // calculate task capacity
        if (!task_capacity) {
            thread_cap = 0;
        } else if (task_capacity > this->thread_numb) {
            this->thread_cap = task_capacity/this->thread_numb;
        } else {
            this->thread_cap = 1;
        }

        // load balance
        this->step_limitation = getBestMoveStep(thread_numb);

        // create
        threads.reset(new DqThread[this->thread_numb]);
        for (int i = 0; i < this->thread_numb; ++i) {
            threads[i].bindHandle(std::thread(&SteadyThreadPond::worker, this, i));
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
            threads[i].join();
        }
    }

    
    /**
     * get block tasks number now
    */
    uint getTasksRemain() 
    {
        uint ret = 0;
        for (int i = 0; i < thread_numb; ++i) {
            ret += threads[i].getTasksNumb();
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
    void waitForTasks() {
        for (int i = 0; i < thread_numb; ++i) {
            threads[i].waitTasksDone();
        }
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
        threads[cur].enqueue(std::forward<_Runable>(foo));
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

        threads[cur].enqueue(std::move(pack));
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
            for (int i = 0; i < size; ) {
                if (threads[cur].getTasksNumb() < thread_cap) {
                    threads[cur].enqueue(std::move(container[i++]));
                } else {
                    util::recyclePlus(cur, 0, thread_numb);
                    if (cur == start) {
                        taskOverFlow(std::forward<_Container>(container), i, size);
                        break;
                    }
                }
            }
            return;
        } 
        threads[cur].enqueue(std::forward<_Container>(container), size);
    }


    // enable task stealing between each thread
    void enableStealTasks(uint max_numb = 0) 
    {
        if (!max_numb) {
            max_numb = std::max(thread_numb/4, 1);
            max_numb = std::min(max_numb, (uint)8);
        }
        if (max_numb >= thread_numb) {
            throw std::invalid_argument("The number of stealing threads must smaller than thread number and greater than zero");
        }
        max_steal = max_numb;
        enable_steal_tasks = true;
    }

    // disable task stealing between each thread
    void disableStealTasks() {
        enable_steal_tasks = false;
    }


private:

    void worker(int index) 
    {   
        auto& self = threads[index];

        while (!stop) 
        {
            // yeild if no tasks
            if (self.notask()) 
            {
                // steal tasks from other threads
                if (enable_steal_tasks) 
                {
                    for (int i = index, j = 0; j < max_steal; j++) 
                    {
                        util::recyclePlus(i, 0, thread_numb);
                        if (threads[i].tryGiveTasks(self)) {
                            self.runBufferTasks();
                            break;
                        } 
                    }
                    if (self.notask()) {
                        std::this_thread::yield();
                        continue;
                    }

                } else {
                    std::this_thread::yield();
                    continue;
                }
            }
            // run tasks 
            self.runTasks();
            
        }
    }


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
        auto spare = [this, tar_capacity] (DqThread& t) {
            return (t.getTasksNumb() + tar_capacity) <= thread_cap;
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
            if (threads[idx].getTasksNumb()) {
                idx = (threads[tmp].getTasksNumb() < threads[idx].getTasksNumb()) ? tmp : idx;
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