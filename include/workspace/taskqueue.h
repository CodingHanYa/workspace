#pragma once
#include <deque>
#include <mutex>

namespace wsp::details {

/**
 * @brief A thread-safe task queue
 * @tparam T runnable object
 * @note The performance of pushing back is better
 */
template <typename T>
class taskqueue {
    std::mutex tq_lok;
    std::deque<T> q;
public:
    using size_type = typename std::deque<T>::size_type;
    taskqueue() = default;
    taskqueue(const taskqueue&) = delete;
    taskqueue(taskqueue&&) = default;
public:
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
    size_type length() {
        std::lock_guard<std::mutex> lock(tq_lok);
        return q.size();
    }
};

} // wsp::details