#pragma once
#include <thread>

namespace wsp {
namespace details {

using AUTO_JOIN = int;
using AUTO_DETACH = unsigned;

template <typename T>
class autothread {};

template <>
class autothread<AUTO_JOIN> {
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
class autothread<AUTO_DETACH> {
    std::thread thrd;
public:
    autothread(std::thread&& t): thrd(std::move(t)) {}
    autothread(const autothread& other) = delete;
    autothread(autothread&& other) = default;
    ~autothread() { if (thrd.joinable()) thrd.detach(); }

    using id = std::thread::id;
    id get_id() { return thrd.get_id(); }
};



} // details
} // wsp