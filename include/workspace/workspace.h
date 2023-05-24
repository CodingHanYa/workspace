#pragma once
#include <cassert>

#include <map>
#include <memory>
#include <vector>
#include <list>

#include <workspace/workbranch.h>
#include <workspace/supervisor.h>

// task type
namespace wsp::task {
    // Possess higher priority then "task::nor"
    using urg = details::urgent;
    // Possess lower  priority then "task::urg"
    using nor = details::normal;  
    // Can be executed by a thread at a time
    using seq = details::sequence;
}

// public
namespace wsp {

// std::future collector
template <typename RT>
using futures = details::futures<RT>;                
// An async working node
using workbranch = details::workbranch;              
// workbranch supervisor
using supervisor = details::supervisor;              

}

namespace wsp {

// Component manager
class workspace {
public:
    class bid {
        workbranch* base = nullptr;
        friend class workspace;
    public:
        bid(workbranch* b)
            : base(b) {}

        bool operator == (const bid& other) {
            return base == other.base;
        }
        bool operator != (const bid& other) {
            return base != other.base;
        }
        bool operator < (const bid& other) {
            return base < other.base;
        }
        friend std::ostream& operator <<(std::ostream& os, const bid& id) {
            os << (uint64_t)(id.base);
            return os;
        }
    };
    class sid {
        supervisor* base = nullptr;
        friend class workspace;
    public:
        sid(supervisor* b)
            : base(b) {}
        
        bool operator == (const sid& other) {
            return base == other.base;
        }
        bool operator != (const sid& other) {
            return base != other.base;
        }
        bool operator < (const sid& other) {
            return base < other.base;
        }
        friend std::ostream& operator <<(std::ostream& os, const sid& id) {
            os << (uint64_t)(id.base);
            return os;
        }
    };
private:
    using branch_lst = std::list<std::unique_ptr<workbranch>>;
    using superv_map = std::map<const supervisor*, std::unique_ptr<supervisor>>;
    using pos_t = branch_lst::iterator;

    pos_t cur = {};
    branch_lst branchs;
    superv_map supervs;

public:
    explicit workspace() = default;
    ~workspace() {
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
        assert(br != nullptr);
        branchs.emplace_back(br);
        cur = branchs.begin();   // reset cursor
        return bid(br);
    }
    /**
     * @brief attach a supervisor
     * @param sp ptr (heap memory)
     * @return id
     * @note O(1)
     */
    sid attach(supervisor* sp) {
        assert(sp != nullptr);
        supervs.emplace(sp, sp);
        return sid(sp);
    }

    /**
     * @brief detach workbranch by id
     * @param id branch's id
     * @return std::unique_ptr<workbranch> 
     * @note O(n)
     */
    auto detach(bid id) -> std::unique_ptr<workbranch> {
        for (auto it = branchs.begin(); it != branchs.end(); it++) {
            if (it->get() == id.base) {
                if (cur == it) forward(cur);
                auto ptr = it->release();
                branchs.erase(it);
                return std::unique_ptr<workbranch>(ptr);
            }
        }
        return nullptr;
    }
    /**
     * @brief detach supervisor by id
     * @param id supervisor's id
     * @return std::unique_ptr<supervisor> 
     * @note O(logn)
     */
    auto detach(sid id) -> std::unique_ptr<supervisor>{
        auto it = supervs.find(id.base);
        if (it == supervs.end()) {
            return nullptr;
        } else {
            auto ptr = it->second.release();
            supervs.erase(it);
            return std::unique_ptr<supervisor>(ptr);
        }
    }
    
    /**
     * @brief travel all the workbranchs and deal each of them
     * @param deal <void(workbranch&)> how to deal with the work branch
     */
    void for_each(std::function<void(workbranch&)> deal) {
        for (auto& each: branchs) {
            deal(*(each.get()));
        }
    }
    /**
     * @brief travel all the supervisors and deal each them
     * @param deal <void(supervisor&)> how to deal with the supervisor
     */
    void for_each(std::function<void(supervisor&)> deal) {
        for (auto& each: supervs) {
            deal(*(each.second.get()));
        }
    }
 
    /**
     * @brief get ref of workbranch by id
     * @param id workbranch's id
     * @return reference of the workbranch
     * @note O(1)
     */
    auto operator [](bid id) -> workbranch& {
        return (*id.base);
    }
    /**
     * @brief get ref of supervisor by id
     * @param id supervisor's id
     * @return reference of the supervisor
     * @note O(1)
     */
    auto operator [](sid id) -> supervisor& {
        return (*id.base);
    }

    /**
     * @brief get reference of workbranch
     * @param id workbranch's id
     * @return reference
     * @note O(1)
     */
    auto get_ref(bid id) -> workbranch& {
        return *id.base;
    }
    /**
     * @brief get reference of supervisor
     * @param id supervisor's id
     * @return reference
     * @note O(1)
     */
    auto get_ref(sid id) -> supervisor& {
        return *id.base;
    }
    
    /**
     * @brief async execute a task
     * @tparam T task type 
     * @param task runnable object
     */
    template <typename T = task::nor, typename F, 
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<std::is_void<R>::value>::type>
    void submit(F&& task) {
        assert(branchs.size() > 0);
        auto this_br = cur->get();
        auto next_br = forward(cur)->get();
        if (next_br->num_tasks() < this_br->num_tasks()) {
            next_br->submit<T>(std::forward<F>(task));
        } else {
            this_br->submit<T>(std::forward<F>(task));
        }
    }
    /**
     * @brief async execute a task
     * @tparam T task type 
     * @tparam R task's return type
     * @param task runnable object
     * @return std::future<R> 
     */
    template <typename T = task::nor, typename F, 
        typename R = details::result_of_t<F>, 
        typename DR = typename std::enable_if<!std::is_void<R>::value, R>::type>
    auto submit(F&& task) -> std::future<R>{
        assert(branchs.size() > 0);
        auto this_br = cur->get();
        auto next_br = forward(cur)->get();
        if (next_br->num_tasks() < this_br->num_tasks()) {
            return next_br->submit<T>(std::forward<F>(task));
        } else {
            return this_br->submit<T>(std::forward<F>(task));
        }
    }
    /**
     * @brief async execute tasks 
     * @param task runnable object (sequnce)
     * @return void
     */
    template <typename T, typename F, typename... Fs>
    auto submit(F&& task, Fs&&... tasks) -> typename std::enable_if<std::is_same<T, task::seq>::value>::type {
        assert(branchs.size() > 0);
        auto this_br = cur->get();
        auto next_br = forward(cur)->get();
        if (next_br->num_tasks() < this_br->num_tasks()) {
            return next_br->submit<T>(std::forward<F>(task), std::forward<Fs>(tasks)...);
        } else {
            return this_br->submit<T>(std::forward<F>(task), std::forward<Fs>(tasks)...);
        }
    }

private:
    const pos_t& forward(pos_t& this_pos) {
        if (++this_pos == branchs.end()) {
            this_pos = branchs.begin(); 
        } 
        return this_pos;
    } 

};

} // wsp
