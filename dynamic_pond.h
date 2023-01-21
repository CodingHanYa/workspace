#pragma once
#include "header.h"


namespace hipe {


/**
 * @brief A dynamic thread pond
*/
class DynamicThreadPond
{
    // thread number
    std::atomic_uint thread_numb = {0};

    // task number
    std::atomic_uint total_tasks = {0};

    // shared task queue
    std::queue<HipeTask> shared_tq = {};

    // locker shared by threads
    std::mutex shared_locker;   

    // stop the pond
    std::atomic_bool stop = {false};

    // waitting for tasks
    std::atomic_bool waiting = {false};

    // cv to awake the paused thread
    std::condition_variable awake_cv;

    // task done
    std::condition_variable task_done_cv;

    // dynamic thread pond
    std::vector<std::thread> pond;

    // expect the size that pond shrink 
    std::atomic_uint shrink_size = {0};

    // the number of deleted threads 
    std::atomic_int dead_thread = {0};

    // deleted thread index
    std::queue<int> deleted_threads;

    // number of the tasks loaded by thread
    std::atomic_uint task_loaded = {0};



public:

    /**
     * @brief construct DynamicThreadPond
     * @param tnumb initial thread number
    */
    DynamicThreadPond(uint tnumb = 0): thread_numb(tnumb)
    {
        for (int i = 0; i < thread_numb; ++i) {
            pond.emplace_back(std::thread(&DynamicThreadPond::worker, this, i));
        }
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
    void close() 
    {
        stop = true;
        awake_cv.notify_all();

        for (auto& thread: pond) {
            thread.join();
        }

    }

    /**
     * @brief add threads
     * @param tnumb thread number
     * The pond will recycle the deleted thread firstly. But if there are  deleted threads, 
     * the pond will expand through creating new thread.
    */
    void addThreads(uint tnumb = 1) 
    {
        int idx = 0;
        while (tnumb--) 
        {
            if (dead_thread) 
            { 
                {   
                    HipeLockGuard lock(shared_locker);
                    idx = deleted_threads.front();
                    deleted_threads.pop();  
                }
                pond[idx].join();   
                dead_thread--;

                pond[idx] = std::thread(&DynamicThreadPond::worker, this, idx);

            } 
            else {
                int idx = pond.size();
                pond.emplace_back(std::thread(&DynamicThreadPond::worker, this, idx));
            }
            thread_numb++;
        }
    }

    /**
     * @brief delete some threads
     * @param tnumb thread number
     * If there are not enough threads, the Hipe will throw error. 
     * The deletion will not happen immediately, but just notify that 
     * there are some threads that need to be deleted, as a result, 
     * it is nonblocking.
    */
    void delThreads(uint tnumb = 1) 
    {
        if (tnumb > thread_numb.load()) {
            util::error("DynamicThreadPond: Not enough threads to delete");
            return;
        }
        shrink_size = tnumb;
        thread_numb -= tnumb;
        awake_cv.notify_all();
    }

    /**
     * @brief adjust thread number to target
     * @param target_tnumb target thread number
    */
    void adjustThreads(uint target_tnumb) 
    {
        if (target_tnumb > thread_numb.load()) {
            addThreads(target_tnumb - thread_numb.load());
        } else {
            delThreads(target_tnumb - thread_numb.load());
        }
    }


    // get task number of the pond, tasks in progress are also counted.
    uint getTasksRemain() {
        return total_tasks.load();
    }

    // get number of the tasks loaded by thread
    uint getTaskLoaded() {
        return task_loaded.load();
    }

    /**
     * reset the number of tasks loaded by thread and return the old value (atomic operation)
     * @return the old value
    */ 
    uint resetTaskLoaded() {
        return task_loaded.exchange(0);
    }

    // get thread number now
    uint getThreadNumb() {
        return thread_numb.load();
    }

    // wait for tasks in the pond done
    void waitForTasks() 
    {
        waiting = true;
        HipeUniqGuard locker(shared_locker);
        task_done_cv.wait(locker, [this]{return !total_tasks;});
        waiting = false;
    }

    /**
     * @brief submit task
     * @param foo An runable object
    */
    template <typename _Runable>
    void submit(_Runable&& foo) 
    {
        {
            HipeLockGuard lock(shared_locker);
            shared_tq.emplace(std::forward<_Runable>(foo));
            ++total_tasks;
        }
        awake_cv.notify_one();
    }

    /**
     * @brief submit task and get return
     * @param foo a runable object
     * @return a future 
    */
    template <typename _Runable>
    auto submitForReturn(_Runable&& foo) -> std::future<typename std::result_of<_Runable()>::type> 
    {
        using RT = typename std::result_of<_Runable()>::type;
        std::packaged_task<RT()> pack(std::move(foo));
        std::future<RT> fut(pack.get_future()); 

        {
            HipeLockGuard lock(shared_locker);
            shared_tq.emplace(std::move(pack));
            ++total_tasks;
        }
        awake_cv.notify_one();
        return fut; // 6
    }


    /**
     * @brief submit same task
     * @param foo  An runable object
     * @param numb The number of task you want to execute
    */
    template <typename _Runable>
    void submit(_Runable&& foo, uint numb) 
    { 
        {
            HipeLockGuard lock(shared_locker);
            total_tasks += numb;
            for (int i = 0; i < numb; ++i) {
                shared_tq.emplace(std::forward<_Runable>(foo));
            }
        }
        awake_cv.notify_all();
    } 


    /**
     * submit in a batch and the task container must override "[]"
     * @param cont task container
     * @param size the size of the container
    */
    template <typename _Container>
    void submitInBatch(_Container& cont, uint size) 
    {
        {
            HipeLockGuard lock(shared_locker);
            total_tasks += size;
            for (int i = 0; i < size; ++i) {
                shared_tq.emplace(std::move(cont[i]));
            }
        }
        awake_cv.notify_all();
    }


private:

    // working threads' default loop
    void worker(uint index) 
    {
        HipeTask task;

        while (!stop) 
        {
            // wait notify 
            HipeUniqGuard locker(shared_locker);
            awake_cv.wait(locker, [this] { return !shared_tq.empty() || stop || shrink_size > 0; });

            if (shrink_size) 
            {
                dead_thread++;
                shrink_size--;
                deleted_threads.push(index);
                break;  
            }
            if (!stop)
            {
                task = std::move(shared_tq.front());
                shared_tq.pop();
                locker.unlock();

                task_loaded++;
                
                // just invoke 
                util::invoke(task);
                --total_tasks;

                if (waiting) {
                    task_done_cv.notify_one();
                }
            }
        }
    } 

};


};