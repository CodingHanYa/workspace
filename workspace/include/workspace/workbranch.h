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
    using lock_guard = std::lock_guard<std::mutex>;
    using worker = autothread<AUTO_DETACH>;
    using worker_map = std::map<worker::id, worker>;

    int decline = 0;
    int running = 0;
    bool destructing = false;
    std::string name = {};

    taskqueue<std::function<void()>> tq = {};
    worker_map workers = {};
    std::mutex branch_lok = {};

    bool waiting_task = false;
    size_t task_done_workers = 0;
    std::condition_variable task_done_cv;
    std::condition_variable thread_cv;

public:
    explicit workbranch(const char* name = "default", int wks = 1) : name(name) {
        for (int i = 0; i < std::max(wks, 1); ++i) {
            add_worker(); // worker 
        }
    }
    workbranch(const workbranch&) = delete;
    workbranch(workbranch&&) = delete;
    ~workbranch() { 
        std::unique_lock<std::mutex> lock(branch_lok);
        decline = running;
        running = 0;
        destructing = true;
        thread_cv.wait(lock, [this]{ return !decline; });
    } 

public:  
    void add_worker() {
        lock_guard lock(branch_lok);
        std::thread t(&workbranch::mission, this);
        workers.emplace(t.get_id(), std::move(t));
        running++;
    }
   
    void del_worker() {
        lock_guard lock(branch_lok);
        if (running <= 0) {
            throw std::runtime_error("workspace: Try to delete too many branchs");
        } else {
            running--;
            decline++;
        }
    }
   
    void wait_tasks(unsigned timeout = -1) {
        {
            std::unique_lock<std::mutex> locker(branch_lok);
            if (!running)  throw std::runtime_error("workspace: No worker executing task");

            waiting_task = true;
            task_done_cv.wait_for(locker, std::chrono::milliseconds(timeout), [this]{
                return task_done_workers >= workers.size();  // use ">=" to avoid supervisor delete workers
            }); 
            task_done_workers = 0;
            waiting_task = false;
        }
        thread_cv.notify_all();  // All thread waiting.
    }

public:
    size_t count_workers() {
        lock_guard lock(branch_lok);
        return workers.size();
    }
    size_t count_tasks() {
        return tq.length();
    }
    const std::string& get_name() {
        return name;
    }

public:
    template <typename T = normal, typename F, typename R = typename std::result_of<F()>::type, typename DR = typename std::enable_if<std::is_void<R>::value>::type>
    auto submit(F&& task) -> typename std::enable_if<std::is_same<T, normal>::value>::type {
        tq.push_back(std::forward<F>(task));
    }

    template <typename T, typename F,  typename R = typename std::result_of<F()>::type, typename DR = typename std::enable_if<std::is_void<R>::value>::type>
    auto submit(F&& task) -> typename std::enable_if<std::is_same<T, urgent>::value>::type {
        tq.push_front(std::forward<F>(task));
    }

    template <typename T = normal, typename F, typename R = typename std::result_of<F()>::type, typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& foo, typename std::enable_if<std::is_same<T, normal>::value, normal>::type = {}) -> std::future<R> {
        std::function<R()> task(std::forward<F>(foo));
        std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
        tq.push_back([task, task_promise] {
            try {
                task_promise->set_value(task());
            } catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                } catch (const std::exception& ex) {
                    std::cerr <<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception: " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr <<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception: " << std::endl;
                }
            }
        });
        return task_promise->get_future();
    }

    template <typename T,  typename F,  typename R = typename std::result_of<F()>::type, typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& foo, typename std::enable_if<std::is_same<T, urgent>::value, urgent>::type = {}) -> std::future<R> {
        std::function<R()> task(std::forward<F>(foo));
        std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
        tq.push_front([task, task_promise] {
            try {
                task_promise->set_value(task());
            } catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                } catch (const std::exception& ex) {
                    std::cerr <<"workspace: worker["<< std::this_thread::get_id()<<"] caught exception: " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr <<"workspace: worker["<< std::this_thread::get_id()<<"] caught unknown exception: " << std::endl;
                }
            }
        });
        return task_promise->get_future();
    }

private:
    void mission() {
        std::function<void()> task;
        while (true) {
            if (decline <= 0 && tq.try_pop(task)) {
                task(); 
            } else if (decline > 0) {
                lock_guard lock(branch_lok);
                if (decline > 0 && decline--) {
                    workers.erase(std::this_thread::get_id());
                    break;
                }
            } else {
                if (waiting_task) { 
                    std::unique_lock<std::mutex> locker(branch_lok);
                    task_done_workers++;
                    task_done_cv.notify_one();
                    thread_cv.wait(locker);
                } else {
                    std::this_thread::yield();
                }
            }
        }
        std::lock_guard<std::mutex> lock(branch_lok);  // lock flag
        if (waiting_task) 
            task_done_cv.notify_one();
        if (destructing) 
            thread_cv.notify_one();
    }

};

} // wsp::details