#pragma once
#include <cassert>
#include <vector>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <workspace/workbranch.h>

namespace wsp::details {

class supervisor {
    using worker = autothread<AUTO_JOIN>;
private:
    bool stop = false;
    bool log = false;

    int wide = 1;
    size_t wmin = 0;
    size_t wmax = 0;
    unsigned tout = 0;
    const unsigned tval = 0;

    std::ostream* output = nullptr;

    std::mutex superv_lok;
    std::mutex output_lok;
    std::vector<worker> thrds;
    std::condition_variable thread_cv;
public:
    /**
     * @brief construct a supervisor
     * @param min_wokrs min nums of workers
     * @param max_wokrs max nums of workers
     * @param time_interval  time interval between each check
     */
    explicit supervisor(int min_wokrs, int max_wokrs, unsigned time_interval = 500)
        : wmin(min_wokrs)
        , wmax(max_wokrs)
        , tout(time_interval)
        , tval(time_interval) 
    {
        assert(min_wokrs >= 0 && max_wokrs > 0);
        while ((max_wokrs /= 10)) { wide++; }
    }
    supervisor(const supervisor&) = delete;
    supervisor(supervisor&&) = delete;
    ~supervisor() { 
        {
            std::lock_guard<std::mutex> lock(superv_lok);
            stop = true;
            thread_cv.notify_all();
        }
    }
    
public:
    /**
     * @brief start supervising a workbranch
     * @param wbr reference of workbranch 
     */
    void supervise(workbranch& wbr) {
        std::lock_guard<std::mutex> lock(superv_lok);
        thrds.emplace_back(std::thread(&supervisor::mission, this, &wbr));
    }
    
    /**
     * @brief suspend the supervisor
     * @param timeout the longest waiting time
     */
    void suspend(unsigned timeout = -1) { 
        std::lock_guard<std::mutex> lock(superv_lok);
        tout = timeout;
    }
    
    /**
     * @brief keep on supervising
     */
    void proceed() {
        {
            std::lock_guard<std::mutex> lock(superv_lok);
            tout = tval;
        }
        thread_cv.notify_one();
    }
    
    /**
     * @brief enable log system
     * @param output output file or std::cout
     */
    void enable_log(std::ostream& output = std::cout) {
        std::lock_guard<std::mutex> lock(output_lok);
        this->output = &output;
    }

    using callback_t = std::function<void(const std::string&, size_t, size_t)>;
    
    /**
     * @brief enable log system
     * @param cb call cb(...) before each relax
     * @note callback_t: (const string& workbranch_name, size_t nums_of_workers, size_t nums_of_tasks)
     */
    void enable_log(callback_t cb) {
        std::lock_guard<std::mutex> lock(output_lok);
        this->cb = cb;
    }

    /**
     * @brief disable log system
     * @note the output port will be wiped
     */
    void disable_log() {
        std::lock_guard<std::mutex> lock(output_lok);
        this->output = nullptr;
    }

private:

    void mission(workbranch* br) {
        while (!stop) {
            auto tknums = br->count_tasks();
            auto wnums = br->count_workers();
            if (tknums) {
                int nums = (int)std::min(wmax-wnums, tknums-wnums);
                for (int i = 0; i < nums; ++i) { 
                    br->add_worker();
                }
            } else if (wnums > wmin) {
                br->del_worker();
            }
            log_info(wnums, tknums, br);
            std::unique_lock<std::mutex> lock(superv_lok);
            if (!stop) thread_cv.wait_for(lock, std::chrono::milliseconds(tout));
        }
    }
    
    callback_t cb = {};

    void log_info(size_t wnums, size_t tknums, workbranch* br) {
        std::lock_guard<std::mutex> lock(output_lok);
        if (cb) {
            cb(br->get_name(), wnums, tknums);
            return; 
        }
        if (output) {
            if (wnums == wmax) {
                (*output)<<"[workspace: "<<br->get_name()<<"] workers: "<<std::left<<std::setw(wide)<<wnums<<" [max] | blocking-task: "<<tknums<<"\n";
            } else if (wnums <= wmin) {
                (*output)<<"[workspace: "<<br->get_name()<<"] workers: "<<std::left<<std::setw(wide)<<wnums<<" [min] | blocking-task: "<<tknums<<"\n";
            } else {
                (*output)<<"[workspace: "<<br->get_name()<<"] workers: "<<std::left<<std::setw(wide)<<wnums<<" [mid] | blocking-task: "<<tknums<<"\n";
            }
        }
    }

};

} // wsp::details