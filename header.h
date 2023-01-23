#pragma once
#include "./util.h"
#include <map>
#include <climits>
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <future>
#include <atomic>
#include <memory>
#include <thread>
#include <cassert>
#include <iostream>
#include <functional>
#include <condition_variable>

namespace hipe {

// ======================
//        alias
// ======================

static const int HipeUnlimited = 0;

/**
 * @brief task type that is able to contain different kinds of executable object!
 * It is quite useful.
*/
using HipeTask = util::Task;

using HipeLockGuard = std::lock_guard<std::mutex>;

using HipeUniqGuard = std::unique_lock<std::mutex>;

using HipeTimePoint = std::chrono::steady_clock::time_point;

template <typename T> 
using HipeFutures  = util::Futures<T>;


}