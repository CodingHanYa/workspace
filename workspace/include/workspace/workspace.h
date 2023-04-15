#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <workspace/workbranch.h>
#include <workspace/supervisor.h>

namespace wsp {

using urg = details::urgent;
using nor = details::normal;

// future collector
template <typename RT>
using futures = details::futures<RT>;

using workbranch = details::workbranch;
using supervisor = details::supervisor;
using worker = details::autothread<details::AUTO_JOIN>;

class workspace {
    std::vector<std::unique_ptr<details::workbranch>> branchs;
    std::vector<std::unique_ptr<details::supervisor>> supervs;
    std::vector<std::unique_ptr<worker>> workers;
    std::mutex space_lok;
    unsigned cur = 0;
    int count = 0;
private:
    struct bid {
        const int var = 0;
        bid(int v): var(v) {}
    };
    struct sid {
        const int var = 0;
        sid(int v): var(v) {}
    };
    struct wid {
        const int var = 0;
        wid(int v): var(v) {}
    };
public:
    explicit workspace() = default;
    ~workspace() {
        workers.clear();
        supervs.clear();
        branchs.clear();
    }
    workspace(const workspace&) = delete;
    workspace(workspace&&) = delete;

    /**
     * @brief attach a workbranch
     * @param br ptr (heap memory)
     * @return id
     * @note O(1)
     */
    bid attach(workbranch* br) {
        std::lock_guard<std::mutex> lock(space_lok);
        branchs.emplace_back(br);
        return bid(branchs.size()-1);
    }
    /**
     * @brief attach a supervisor
     * @param br ptr (heap memory)
     * @return id
     * @note O(1)
     */
    sid attach(supervisor* sp) {
        std::lock_guard<std::mutex> lock(space_lok);
        supervs.emplace_back(sp);
        return sid(supervs.size()-1);
    }
    /**
     * @brief attach a worker
     * @param wk ptr (heap memory)
     * @return id
     * @note O(1)
     */
    wid attach(worker* wk) {
        std::lock_guard<std::mutex> lock(space_lok);
        workers.emplace_back(wk);
        return wid(workers.size()-1);
    }


    /**
     * @brief detach workbranch by id
     * @param id branch's id
     * @return std::unique_ptr<workbranch> 
     * @note O(1)
     */
    auto detach(bid id) -> std::unique_ptr<workbranch> {
        std::lock_guard<std::mutex> lock(space_lok);
        auto res = branchs[id.var].release();
        branchs[id.var].reset(branchs.back().release());
        branchs.pop_back();
        return std::unique_ptr<workbranch>(res);
    }
    /**
     * @brief detach supervisor by id
     * @param id supervisor's id
     * @return std::unique_ptr<supervisor> 
     * @note O(1)
     */
    auto detach(sid id) -> std::unique_ptr<supervisor>{
        std::lock_guard<std::mutex> lock(space_lok);
        auto res = supervs[id.var].release();
        supervs[id.var].reset(supervs.back().release());
        supervs.pop_back();
        return std::unique_ptr<supervisor>(res);
    }
    /**
     * @brief detach worker by id
     * @param id worker's id
     * @return std::unique_ptr<worker> 
     * @note O(1)
     */
    auto detach(wid id) -> std::unique_ptr<worker> {
        std::lock_guard<std::mutex> lock(space_lok);
        auto res = workers[id.var].release();
        workers[id.var].reset(workers.back().release());
        workers.pop_back();
        return std::unique_ptr<worker>(res);
    }


    /**
     * @brief travel all the workbranchs and deal each branch
     * @param deal how to deal with the work branch
     */
    void for_each(std::function<void(workbranch&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = 0; i < (int)branchs.size(); ++i) {
            deal(*(branchs[i].get()));
        }
    }
    /**
     * @brief travel workbranchs and deal each branch
     * @param first the first work branch
     * @param deal how to deal with the work branch
     */
    void for_each(bid first, std::function<void(workbranch&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)branchs.size(); ++i) {
            deal(*(branchs[i].get()));
        }
    }
    /**
     * @brief travel workbranchs and deal each branch
     * @param first the first work branch
     * @param last the last work branch
     * @param deal how to deal with the work branch
     */
    void for_each(bid first, bid last, std::function<void(workbranch&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)branchs.size(); ++i) {
            deal(*(branchs[i].get()));
        }
    }
    

    /**
     * @brief travel all the supervisors and deal each supervisor
     * @param deal how to deal with the supervisor
     */
    void for_each(std::function<void(supervisor&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = 0; i < (int)supervs.size(); ++i) {
            deal(*(supervs[i].get()));
        }
    }
    /**
     * @brief travel supervisors and deal each supervisor
     * @param first the first supervisor
     * @param deal how to deal with the supervisor
     */
    void for_each(sid first, std::function<void(supervisor&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)supervs.size(); ++i) {
            deal(*(supervs[i].get()));
        }
    }
    /**
     * @brief travel supervisors and deal each supervisor
     * @param first the first supervisor
     * @param last the last supervisor (not included)
     * @param deal how to deal with the supervisor
     */
    void for_each(sid first, sid last, std::function<void(supervisor&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)supervs.size(); ++i) {
            deal(*(supervs[i].get()));
        }
    }
    
    
    /**
     * @brief travel all the workers and deal each worker
     * @param deal how to deal with the worker
     */
    void for_each(std::function<void(worker&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = 0; i < (int)workers.size(); ++i) {
            deal(*(workers[i].get()));
        }
    }
    /**
     * @brief travel workers and deal each worker
     * @param first the first worker
     * @param deal how to deal with the worker
     */
    void for_each(wid first, std::function<void(worker&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)workers.size(); ++i) {
            deal(*(workers[i].get()));
        }
    }
    /**
     * @brief travel workers and deal each worker
     * @param first the first worker
     * @param last the last worker (not included)
     * @param deal how to deal with the worker
     */
    void for_each(wid first, wid last, std::function<void(worker&)> deal) {
        std::lock_guard<std::mutex> lock(space_lok);
        for (int i = first.var; i < (int)workers.size(); ++i) {
            deal(*(workers[i].get()));
        }
    }
    

    /**
     * @brief get ref of workbranch by id
     * @param id workbranch's id
     * @return reference of the workbranch
     */
    auto operator [](bid id) -> workbranch& {
        std::lock_guard<std::mutex> lock(space_lok);
        return *(branchs[id.var].get());
    }
    /**
     * @brief get ref of supervisor by id
     * @param id supervisor's id
     * @return reference of the supervisor
     */
    auto operator [](sid id) -> supervisor& {
        std::lock_guard<std::mutex> lock(space_lok);
        return *(supervs[id.var].get());
    }
    /**
     * @brief get ref of worker by id
     * @param id worker's id
     * @return reference of the worker
     */
    auto operator [](wid id) -> worker& {
        std::lock_guard<std::mutex> lock(space_lok);
        return *(workers[id.var].get());
    }


    /**
     * @brief get reference of workbranch
     * @param id workbranch's id
     * @return reference
     */
    auto get_ref(bid id) -> workbranch& {
        return *(branchs[id.var].get());
    }
    /**
     * @brief get reference of supervisor
     * @param id supervisor's id
     * @return reference
     */
    auto get_ref(sid id) -> supervisor& {
        return *(supervs[id.var].get());
    }
    /**
     * @brief get reference of worker
     * @param id worker's id
     * @return reference
     */
    auto get_ref(wid id) -> worker& {
        return *(workers[id.var].get());
    }

    template <typename T = nor, typename F, typename R = typename std::result_of<F()>::type, typename D = typename std::enable_if<std::is_void<R>::value>::type>
    void submit(F&& task) {
        std::lock_guard<std::mutex> lock(space_lok);
        auto& next_br = branchs[(cur+1) % branchs.size()];
        auto& this_br = branchs[cur % branchs.size()];
        cur++;  // move to next branch
        if (next_br->count_tasks() < this_br->count_tasks()) {
            next_br->submit(std::forward<F>(task));
        } else {
            this_br->submit(std::forward<F>(task));
        }
    }

    template <typename T = nor, typename F, typename R = typename std::result_of<F()>::type, typename D = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& task) -> std::future<R> {
        std::lock_guard<std::mutex> lock(space_lok);
        auto& next_br = branchs[(cur+1) % branchs.size()];
        auto& this_br = branchs[cur % branchs.size()];
        cur++; // move to next branch
        if (next_br->count_tasks() < this_br->count_tasks()) {
            return next_br->submit(std::forward<F>(task));
        } else {
            return this_br->submit(std::forward<F>(task));
        }
    }

};



} // namespace space
