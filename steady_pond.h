#pragma once
#include "header.h"

// In benchmark/compare_batch_submit.cpp , we use 'count' to know which thread is faster ?  main thread or worker thread ?
//static int count = 0;

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

        
        //if (buffer_tq.size() > 10) {
        //    count++;
        //}

        while (!buffer_tq.empty()) {
            util::invoke(buffer_tq.front());
            buffer_tq.pop();
            task_numb--;
        }
    }

    void runBufferTasks() 
    {
        while (!buffer_tq.empty()) {
            util::invoke(buffer_tq.front());
            buffer_tq.pop();
            task_numb--;
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
                tq_locker.unlock();
                task_numb -= numb;
                t.task_numb += numb;
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
 * Support task stealing and execute tasks in batches
*/
class SteadyThreadPond: public FixedThreadPondBase<DqThread>
{
public:

    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
    */
    SteadyThreadPond(uint thread_numb = 0, uint task_capacity = HipeUnlimited) 
        : FixedThreadPondBase(thread_numb, task_capacity)
    {
        // create threads
        threads.reset(new DqThread[this->thread_numb]);
        for (int i = 0; i < this->thread_numb; ++i) {
            threads[i].bindHandle(std::thread(&SteadyThreadPond::worker, this, i));
        }
    }
    
    ~SteadyThreadPond() {}

private:

    void worker(int index) 
    {   
        auto& self = threads[index];

        while (!stop) 
        {
            // yeild if no tasks
            if (self.notask()) 
            {
                // notify the main thread
                if (self.isWaiting()) {
                    self.notifyTaskDone();
                    std::this_thread::yield();
                    continue;
                }
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
                    if (!self.notask() || self.isWaiting()) {
                        // go to handle the tasks or the waiting signal directly
                        continue;
                    }
                }
                std::this_thread::yield();

            } else {
                // run tasks 
                self.runTasks();
            }
            
        }
    }


};

}