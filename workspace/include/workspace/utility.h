#pragma once
#include <future>
#include <functional>
#include <deque>
#include <chrono>
#include <cstdlib>

namespace wsp::details {

using sz_t = size_t;

struct normal {};  // normal task
struct urgent {};  // urgent task
struct sequence {}; // sequence tasks

/**
 * @brief set of std::future
 * @tparam T return type
 */
template <typename T>
class futures {
    std::deque<std::future<T>> futs;
public:
    using iterator = typename std::deque<std::future<T>>::iterator;
    void wait() {
        for (auto& each: futs) {
            each.wait();
        }
    }
    sz_t size() {
        return futs.size();
    }
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
