#pragma once
#include <cassert>
#include <vector>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <workspace/workbranch.h>
#include <workspace/utility.h>

namespace wsp::details {

// shared by all supervisors
static std::mutex spv_lok;

// workbranch supervisor
class supervisor {
    using worker = autothread<join>;
    using callback_t = std::function<void()>;
private:
    bool stop = false;
    bool log = false;

    int  wide = 1;
    sz_t wmin = 0;
    sz_t wmax = 0;
    unsigned tout = 0;
    const unsigned tval = 0;

    callback_t tick_cb = {};
    std::ostream* output = nullptr;

    std::vector<worker> thrds;
    std::condition_variable thrd_cv;
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
        assert(min_wokrs >= 0 && max_wokrs > 0 && max_wokrs > min_wokrs);
        while ((max_wokrs /= 10)) { wide++; }
    }
    supervisor(const supervisor&) = delete;
    supervisor(supervisor&&) = delete;
    ~supervisor() { 
        {
            std::lock_guard<std::mutex> lock(spv_lok);
            stop = true;
            thrd_cv.notify_all();
        }
    }
    
public:
    /**
     * @brief start supervising a workbranch
     * @param wbr reference of workbranch 
     */
    void supervise(workbranch& wbr) {
        std::lock_guard<std::mutex> lock(spv_lok);
        thrds.emplace_back(std::thread(&supervisor::mission, this, &wbr));
    }
    
    /**
     * @brief suspend the supervisor
     * @param timeout the longest waiting time
     */
    void suspend(unsigned timeout = -1) { 
        std::lock_guard<std::mutex> lock(spv_lok);
        tout = timeout;
    }
    
    // go on supervising
    void proceed() {
        {
            std::lock_guard<std::mutex> lock(spv_lok);
            tout = tval;
        }
        thrd_cv.notify_all();
    }
    
    /**
     * @brief enable log system
     * @param output output file or std::cout
     */
    void enable_log(std::ostream& output = std::cout) {
        std::lock_guard<std::mutex> lock(spv_lok);
        this->output = &output;
    }

    /**
     * @brief Always execute callback before taking a rest
     * @param cb callback function
     */
    void set_tick_cb(std::function<void()> cb) {
        std::lock_guard<std::mutex> lock(spv_lok);
        tick_cb = cb;
    }
    
    /**
     * @brief disable log system
     * @note the output port will be wiped
     */
    void disable_log() {
        std::lock_guard<std::mutex> lock(spv_lok);
        this->output = nullptr;
    }
private:

    void mission(workbranch* br) {
        while (!stop) {
            try {
                // get info
                auto tknums = br->num_tasks();
                auto wknums = br->num_workers();
                // adjust
                if (tknums) {
                    sz_t nums = std::min(wmax-wknums, tknums-wknums);
                    for (sz_t i = 0; i < nums; ++i) { 
                        br->add_worker(); // quick add
                    }
                } else if (wknums > wmin) {
                    br->del_worker();     // slow dec
                }
                // log
                std::unique_lock<std::mutex> lock(spv_lok);
                if (tick_cb) 
                    tick_cb();
                if (output) {
                    if (wknums == wmax) {
                        (*output)<<"workspace: "<<br->get_name()<<" workers: "
                        <<std::left<<std::setw(wide)<<wknums<<" [max] | blocking-task: "<<tknums<<"\n";
                    } else if (wknums <= wmin) {
                        (*output)<<"workspace: "<<br->get_name()<<" workers: "
                        <<std::left<<std::setw(wide)<<wknums<<" [min] | blocking-task: "<<tknums<<"\n";
                    } else {
                        (*output)<<"workspace: "<<br->get_name()<<" workers: "
                        <<std::left<<std::setw(wide)<<wknums<<" [mid] | blocking-task: "<<tknums<<"\n";
                    }
                }
                // task a rest
                if (!stop) 
                    thrd_cv.wait_for(lock, std::chrono::milliseconds(tout));
            } catch (const std::exception& e) {
                std::cerr<<"workspace: supervisor["<< std::this_thread::get_id()<<
                "] caught exception:\n  what(): "<<e.what()<<'\n'<<std::flush;
            }
        }
    }

};

} // wsp::details