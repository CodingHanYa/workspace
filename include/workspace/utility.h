#pragma once
#include <future>
#include <functional>
#include <deque>
#include <cstdlib>
#include <type_traits>

namespace wsp::details {

// size type
using sz_t = size_t;

// type trait
#if __cplusplus >= 201703L 
template <typename F, typename... Args>
using result_of_t = std::invoke_result_t<F, Args...>; 
#else
template <typename F, typename... Args>
using result_of_t = typename std::result_of<F(Args...)>::type; 
#endif

// type trait
struct normal   {};  // normal task (for type inference)
struct urgent   {};  // urgent task (for type inference)
struct sequence {};  // sequence tasks (for type inference)

/**
 * @brief std::future collector
 * @tparam T return type
 */
template <typename T>
class futures {
    std::deque<std::future<T>> futs;
public:
    using iterator = typename std::deque<std::future<T>>::iterator;
    
    // wait for all futures
    void wait() {
        for (auto& each: futs) {
            each.wait();
        }
    }
    sz_t size() {
        return futs.size();
    }
    /**
     * @brief get set of result
     * @return std::vector<T>
     */
    auto get() -> std::vector<T> {
        std::vector<T> res;
        for (auto& each: futs) {
            res.emplace_back(each.get());
        }
        return res;
    }
    auto end() -> iterator& {
        return futs.end();
    }
    auto begin()->iterator& {
        return futs.begin();
    }
    void add_back(std::future<T>&& fut) {
        futs.emplace_back(std::move(fut));
    }
    void add_front(std::future<T>&& fut) {
        futs.emplace_front(std::move(fut));
    }
    void for_each(std::function<void(std::future<T>&)> deal) {
        for (auto& each: futs) {
            deal(each);
        }
    }
    void for_each(const iterator& first, std::function<void(std::future<T>&)> deal) {
        for (auto it = first; it != end(); ++it) {
            deal(*it);
        }
    }
    void for_each(const iterator& first, const iterator& last, std::function<void(std::future<T>&)> deal) {
        for (auto it = first; it != last; ++it) {
            deal(*it);
        }
    }
    auto operator [] (sz_t idx) -> std::future<T>& {
        return futs[idx];
    }
};

} // wsp::details
