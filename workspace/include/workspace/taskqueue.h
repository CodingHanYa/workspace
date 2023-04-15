#pragma once
#include <deque>
#include <mutex>
#include <cstdlib>

namespace wsp {
namespace details {

/**
 * @brief A thread-safe task queue
 * @tparam T runnable object
 * @note While pushing a sequence, the begin of the sequence will always be firstly got by worker.
 */
template <typename T>
class taskqueue {
    std::mutex tq_lok;
    std::deque<T> q;
public:
    using sz_t = size_t;
    taskqueue() = default;
    taskqueue(taskqueue&&) = default;

    void push_back(T& v) {
        std::lock_guard<std::mutex> lock(tq_lok);
        q.emplace_back(v);
    }
    void push_back(T&& v) {
        std::lock_guard<std::mutex> lock(tq_lok);
        q.emplace_back(std::move(v));
    }
    void push_front(T& v) {
        std::lock_guard<std::mutex> lock(tq_lok);
        q.emplace_front(v);
    }
    void push_front(T&& v) {
        std::lock_guard<std::mutex> lock(tq_lok);
        q.emplace_front(std::move(v));
    }
    bool try_pop(T& tmp) {
        std::lock_guard<std::mutex> lock(tq_lok);
        if (!q.empty()) {
            tmp = std::move(q.front());
            q.pop_front();
            return true;
        }
        return false;
    }
    sz_t length() {
        std::lock_guard<std::mutex> lock(tq_lok);
        return q.size();
    }
};

} // details
} // wsp