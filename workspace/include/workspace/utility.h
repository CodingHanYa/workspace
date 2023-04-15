#pragma once
#include <future>
#include <functional>
#include <deque>
#include <chrono>
#include <cstdlib>

namespace wsp::details {

struct normal {};
struct urgent {};

/**
 * @brief set of std::future
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
    // get results
    auto get() -> std::vector<T> {
        std::vector<T> res;
        wait();
        for (auto& each: futs) {
            res.emplace_back(each.get());
        }
        return res;
    }
    size_t size() {
        return futs.size();
    }
    iterator& begin() {
        return futs.begin();
    }
    iterator& end() {
        return futs.end();
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
    void add_back(std::future<T>&& fut) {
        futs.emplace_back(std::move(fut));
    }
    void add_front(std::future<T>&& fut) {
        futs.emplace_front(std::move(fut));
    }
};


} // wsp::details
