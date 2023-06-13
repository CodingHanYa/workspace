#pragma once
#include <cstdlib>

#include <map>
#include <memory>
#include <future>
#include <iostream>
#include <condition_variable>

#include <workspace/taskqueue.h>
#include <workspace/autothread.h>
#include <workspace/utility.h>

namespace wsp::details {

class workbranch {
    using worker = autothread<detach>;
    using worker_map = std::map<worker::id, worker>;

    sz_t decline = 0;
    sz_t task_done_workers = 0;
    bool is_waiting = false;
    bool destructing = false;

    worker_map workers = {};
    taskqueue<std::function<void()>> tq = {};

    std::mutex lok = {};
    std::condition_variable thread_cv = {};
    std::condition_variable task_done_cv = {};

public:
    /**
     * @brief construct function
     * @param wks initial number of workers
     */
    explicit workbranch(int wks = 1) {
        for (int i = 0; i < std::max(wks, 1); ++i) {
            add_worker(); // worker 
        }
    }
    workbranch(const workbranch&) = delete;
    workbranch(workbranch&&) = delete;
    ~workbranch() { 
        std::unique_lock<std::mutex> lock(lok);
        decline = workers.size();
        destructing = true;
        thread_cv.wait(lock, [this]{ return !decline; });
    } 

public:  
    /**
     * @brief add one worker
     * @note O(logN)
     */
    void add_worker() {
        std::lock_guard<std::mutex> lock(lok);
        std::thread t(&workbranch::mission, this);
        workers.emplace(t.get_id(), std::move(t));
    }
    
    /**
     * @brief delete one worker
     * @note O(1)
     */
    void del_worker() {
        std::lock_guard<std::mutex> lock(lok);
        if (workers.empty()) {
            throw std::runtime_error("workspace: No worker in workbranch to delete");
        } else {
            decline++;
        }
    }
   
    /**
     * @brief Wait for all tasks done.
     * @brief This interface will pause all threads(workers) in workbranch to relieve system's stress.
     * @param timeout timeout for waiting
     * @return return true if all tasks done 
     */
    bool wait_tasks(unsigned timeout = -1) {
        bool res;
        {
            std::unique_lock<std::mutex> locker(lok);
            is_waiting = true;
            res = task_done_cv.wait_for(locker, std::chrono::milliseconds(timeout), [this]{
                return task_done_workers >= workers.size();  // use ">=" to avoid supervisor delete workers
            }); 
            task_done_workers = 0;
            is_waiting = false;
        }
        thread_cv.notify_all();  // recover
        return res;
    }

public:
    /**
     * @brief get number of workers
     * @return number 
     */
    sz_t num_workers() {
        std::lock_guard<std::mutex> lock(lok);
        return workers.size();
    }
    /**
     * @brief get number of tasks in the task queue
     * @return number 
     */
    sz_t num_tasks() {
        return tq.length();
    }

public:
    /**
     * @brief async execute the task 
     * @param task runnable object (normal)
     * @return void
     */
    template <typename T = normal, typename F, 
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<std::is_void<R>::value>::type>
    auto submit(F&& task) -> typename std::enable_if<std::is_same<T, normal>::value>::type {
        tq.push_back([task]{
            try {
                task();
            } catch (const std::exception& ex) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception:\n  what(): "<<ex.what()<<'\n'<<std::flush;
            } catch (...) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception\n"<<std::flush;
            }
        });
    }

    /**
     * @brief async execute the task 
     * @param task runnable object (urgent)
     * @return void
     */
    template <typename T, typename F,  
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<std::is_void<R>::value>::type>
    auto submit(F&& task) -> typename std::enable_if<std::is_same<T, urgent>::value>::type {
        tq.push_front([task]{
            try {
                task();
            } catch (const std::exception& ex) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception:\n  what(): "<<ex.what()<<'\n'<<std::flush;
            } catch (...) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception\n"<<std::flush;
            }
        });
    }

    /**
     * @brief async execute tasks 
     * @param task runnable object (sequence)
     * @return void
     */
    template <typename T, typename F, typename... Fs>
    auto submit(F&& task, Fs&&... tasks) -> typename std::enable_if<std::is_same<T, sequence>::value>::type {
        tq.push_back([=]{
            try {
                this->rexec(task, tasks...);
            } catch (const std::exception& ex) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception:\n  what(): "<<ex.what()<<'\n'<<std::flush;
            } catch (...) {
                std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception\n"<<std::flush;
            }
        });
    }

    /**
     * @brief async execute the task
     * @param task runnable object (normal)
     * @param dummy dummy
     * @return std::future<R>
     */
    template <typename T = normal, typename F, 
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& task, typename std::enable_if<std::is_same<T, normal>::value, normal>::type = {}) -> std::future<R> {
        std::function<R()> exec(std::forward<F>(task));
        std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
        tq.push_back([exec, task_promise] {
            try {
                task_promise->set_value(exec());
            } catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                } catch (const std::exception& ex) {
                    std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception:\n  what(): "<<ex.what()<<'\n'<<std::flush;
                } catch (...) {
                    std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception\n"<<std::flush;
                }
            }
        });
        return task_promise->get_future();
    }
    
    /**
     * @brief async execute the task
     * @param task runnable object (urgent)
     * @param dummy dummy
     * @return std::future<R>
     */
    template <typename T,  typename F,  
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& task, typename std::enable_if<std::is_same<T, urgent>::value, urgent>::type = {}) -> std::future<R> {
        std::function<R()> exec(std::forward<F>(task));
        std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
        tq.push_front([exec, task_promise] {
            try {
                task_promise->set_value(exec());
            } catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                } catch (const std::exception& ex) {
                    std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception:\n  what(): "<<ex.what()<<'\n'<<std::flush;
                } catch (...) {
                    std::cerr<<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception\n"<<std::flush;
                }
            }
        });
        return task_promise->get_future();
    }

private:
    // loop
    void mission() {
        std::function<void()> task;
        while (true) {
            if (decline <= 0 && tq.try_pop(task)) {
                task(); 
            } else if (decline > 0) {
                std::lock_guard<std::mutex> lock(lok);
                if (decline > 0 && decline--) {
                    workers.erase(std::this_thread::get_id());
                    if (is_waiting)  
                        task_done_cv.notify_one();
                    if (destructing) 
                        thread_cv.notify_one();
                    return;
                }
            } else {
                if (is_waiting) { 
                    std::unique_lock<std::mutex> locker(lok);
                    task_done_workers++;
                    task_done_cv.notify_one();
                    thread_cv.wait(locker);
                } else {
                    std::this_thread::yield();
                }
            }
        }
    }

    // recursive execute 
    template <typename F>
    void rexec(F&& func) {
        func();
    }
    
    // recursive execute 
    template <typename F, typename... Fs>
    void rexec(F&& func, Fs&&... funcs) {
        func();
        rexec(std::forward<Fs>(funcs)...);
    }
};

} // wsp::details