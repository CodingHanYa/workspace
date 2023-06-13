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

// workbranch supervisor
class supervisor {
    using tick_callback_t = std::function<void()>;
private:
    bool stop = false;

    sz_t wmin = 0;
    sz_t wmax = 0;
    unsigned tout = 0;
    const unsigned tval = 0;

    tick_callback_t tick_cb = {};

    autothread<join> worker;
    std::vector<workbranch*> branchs;
    std::condition_variable thrd_cv;
    std::mutex spv_lok;
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
        , tick_cb([]{})
        , worker(std::thread(&supervisor::mission, this))
    {
        assert(min_wokrs >= 0 && max_wokrs > 0 && max_wokrs > min_wokrs);
    }
    supervisor(const supervisor&) = delete;
    supervisor(supervisor&&) = delete;
    ~supervisor() { 
        {
            std::lock_guard<std::mutex> lock(spv_lok);
            stop = true;
            thrd_cv.notify_one();
        }
    }
    
public:
    /**
     * @brief start supervising a workbranch
     * @param wbr reference of workbranch 
     */
    void supervise(workbranch& wbr) {
        std::lock_guard<std::mutex> lock(spv_lok);
        branchs.emplace_back(&wbr);
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
        thrd_cv.notify_one();
    }
    /**
     * @brief Always execute callback before taking a rest
     * @param cb callback function
     */
    void set_tick_cb(tick_callback_t cb) {
        tick_cb = cb;
    }
private:
    // loop func
    void mission() {
        while (!stop) {
            try {
                {
                    std::unique_lock<std::mutex> lock(spv_lok);
                    for (auto pbr: branchs) {
                        // get info
                        auto tknums = pbr->num_tasks();
                        auto wknums = pbr->num_workers();
                        // adjust
                        if (tknums) { 
                            sz_t nums = std::min(wmax-wknums, tknums-wknums);
                            for (sz_t i = 0; i < nums; ++i) { 
                                pbr->add_worker(); // quick add
                            }
                        } else if (wknums > wmin) {
                            pbr->del_worker();     // slow dec
                        }
                    }
                    if (!stop) thrd_cv.wait_for(lock, std::chrono::milliseconds(tout));
                }
                tick_cb();  // execute tick callback

            } catch (const std::exception& e) {
                std::cerr<<"workspace: supervisor["<< std::this_thread::get_id()<<"] caught exception:\n  \
                what(): "<<e.what()<<'\n'<<std::flush;
            }
        }
    }

};

} // wsp::details