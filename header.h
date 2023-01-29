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
protected:

    bool waiting = false;
    std::thread handle;

    std::atomic_int task_numb = {0};
    std::condition_variable task_done_cv;
    std::mutex cv_locker;

public:

    uint getTasksNumb() {
        return task_numb.load();
    }

    bool notask() {
        return !task_numb;
    }

    void join() {
        handle.join();
    }

    void bindHandle(std::thread&& handle) {
        this->handle = std::move(handle);
    }

    bool isWaiting() {
        return waiting;
    }

    void waitTasksDone() {
        waiting = true;
        HipeUniqGuard lock(cv_locker);
        task_done_cv.wait(lock, [this]{ return !task_numb; });
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
 * @tparam The type of thread wrepper class that inherited from ThreadBase.
*/
template <typename Ttype>
class FixedThreadPondBase 
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
    uint thread_cap = 0;

    // tasks that failed to submit
    util::Block<HipeTask> overflow_tasks = {0};

    // task overflow call back
    HipeTask refuse_cb;

protected:

    /**
     * @param thread_numb fixed thread number
     * @param task_capacity task capacity of the pond, default: unlimited
    */
    FixedThreadPondBase(uint thread_numb = 0, uint task_capacity = HipeUnlimited) 
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
        cursor_move_limit = getBestMoveLimit(thread_numb);
    }

    virtual ~FixedThreadPondBase() {
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
     * get the number of threads 
    */
    uint getThreadNumb() {
        return thread_numb;
    }

    /**
     * @brief submit task
     * @param foo a runable object
    */
    template <typename _Runable>
    void submit(_Runable&& foo) 
    {
        if (!admit()) {
            taskOverFlow(std::forward<_Runable>(foo));
            return;
        }
        auto t = getLeastBusyThread();
        t->enqueue(std::forward<_Runable>(foo));
    }

    /**
     * @brief submit task and get return
     * @param foo a runable object
     * @return a future 
    */
    template <typename _Runable>
    auto submitForReturn(_Runable&& foo) -> std::future<typename std::result_of<_Runable()>::type> 
    {
        if (!admit()) {
            taskOverFlow(std::forward<_Runable>(foo));
            return;
        }

        using RT = typename std::result_of<_Runable()>::type;
        std::packaged_task<RT()> pack(std::move(foo));
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
    template <typename _Container>
    void submitInBatch(_Container&& container, uint size) 
    {
        if (thread_cap) 
        {
            moveCursorToLeastBusy();
            int start = cursor;
            for (int i = 0; i < size; ++i) 
            {
                // admit one task
                if (admit()) {
                    getThreadNow()->enqueue(std::move(container[i]));
                } else {
                    taskOverFlow(std::forward<_Container>(container), i, size);
                    break;
                }
            }

        } else {
            getLeastBusyThread()->enqueue(std::forward<_Container>(container), size);
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
    Ttype* getLeastBusyThread() 
    {
        moveCursorToLeastBusy();
        return &threads[cursor];
    }

    /**
     * Move cursor to the least busy thread. 
     * If the thread that pointed by the cursor has been the least busy one then the cursor will not move.
    */ 
    void moveCursorToLeastBusy()
    {
        int tmp = cursor;
        for (int i = 0; i < cursor_move_limit; ++i) 
        {
            if (threads[cursor].getTasksNumb()) {
                cursor = (threads[tmp].getTasksNumb() < threads[cursor].getTasksNumb()) ? tmp : cursor;
                util::recyclePlus(tmp, 0, thread_numb);
            } else {
                break;
            }
        }

    }

    // calculate best cursor move limit
    int getBestMoveLimit(int thread_numb) 
    {
        if (thread_numb == 1) {
            return 0;
        }
        int tmp = thread_numb / 4;
        tmp = (tmp < 1) ? 1 : tmp;
        return (tmp > 4) ? 4 : tmp;
    }

public:

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


public:

    // ====================================================
    //               task overflow mechanism
    // ====================================================


    /**
     * Set refuse call back. 
     * If the capacity is unlimited , the hipe will throw a logic error. 
     * If didn't set and refuse call back, the hipe will throw logic error and abort the program. 
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
     * Pull the overflow tasks managed by a block which will be kept until next task overflow. 
     * Then the new tasks will replace the old.
    */
    util::Block<HipeTask>  pullOverFlowTasks() {
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
    bool admit(uint tar_capacity = 1) 
    {
        if (!thread_cap) {
            return true;
        }
        int prev = cursor;
        auto spare = [this, tar_capacity] (Ttype& t) {
            return (t.getTasksNumb() + tar_capacity) <= thread_cap;
        };
        while (!spare(threads[cursor])) 
        {
            util::recyclePlus(cursor, 0, thread_numb);
            if (cursor == prev) return false;
        }
        return true;
    }

    


    // task overflow callback for one task
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
     * task overflow callback for batch submit
     * @param left left edge(included)
     * @param right right edge(not included)
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

};

}