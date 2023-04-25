#pragma once
#include <thread>

namespace wsp::details {

struct join {};   // just for type inference
struct detach {}; // just for type inference

// thread wrapper
template <typename T>
class autothread {};

template <>
class autothread<join> {
    std::thread thrd;
public:
    autothread(std::thread&& t): thrd(std::move(t)) {}
    autothread(const autothread& other) = delete;
    autothread(autothread&& other) = default;
    ~autothread() { if (thrd.joinable()) thrd.join(); }

    using id = std::thread::id;
    id get_id() { return thrd.get_id(); }
};

template <>
class autothread<detach> {
    std::thread thrd;
public:
    autothread(std::thread&& t): thrd(std::move(t)) {}
    autothread(const autothread& other) = delete;
    autothread(autothread&& other) = default;
    ~autothread() { if (thrd.joinable()) thrd.detach(); }

    using id = std::thread::id;
    id get_id() { return thrd.get_id(); }
};

} // wsp::details