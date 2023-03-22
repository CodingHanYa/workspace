#pragma once
#include <atomic>
#include <cstddef>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace hipe {

// util for hipe
namespace util {

// ======================
//       Easy sleep
// ======================

inline void sleep_for_seconds(int sec) {
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

inline void sleep_for_milli(int milli) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

inline void sleep_for_micro(int micro) {
    std::this_thread::sleep_for(std::chrono::microseconds(micro));
}

inline void sleep_for_nano(int nano) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

// ======================
//        Easy IO
// ======================

template <typename T>
void print(T&& t) {
    std::cout << std::forward<T>(t) << std::endl;
}

template <typename T, typename... Args>
void print(T&& t, Args&&... argv) {
    std::cout << std::forward<T>(t);
    print(std::forward<Args>(argv)...);
}

/**
 * Thread sync output stream.
 * It can protect the output from multi thread competition.
 */
class SyncStream {
    std::ostream& out_stream;
    std::recursive_mutex io_locker;

public:
    explicit SyncStream(std::ostream& out_stream = std::cout)
      : out_stream(out_stream) {
    }
    template <typename T>
    void print(T&& items) {
        io_locker.lock();
        out_stream << std::forward<T>(items) << std::endl;
        io_locker.unlock();
    }
    template <typename T, typename... A>
    void print(T&& item, A&&... items) {
        io_locker.lock();
        out_stream << std::forward<T>(item);
        this->print(std::forward<A>(items)...);
        io_locker.unlock();
    }
};

// ===========================
//            TMP
// ===========================

// judge whether template param is a runnable object
template <typename F, typename... Args>
using is_runnable = std::is_constructible<std::function<void(Args...)>, typename std::remove_reference<F>::type>;

// judge whether whether the runnable object F's return type is R
template <typename F, typename R>
using is_return = std::is_same<typename std::result_of<F()>::type, R>;

// judge whether the 'U' is std::reference_wrapper<...>
template <typename U, typename DU = typename std::decay<U>::type>
struct is_reference_wrapper {
    template <typename T, typename D = typename T::type>
    static constexpr bool check(T*) {
        return std::is_same<T, std::reference_wrapper<D>>::value;
    };
    static constexpr bool check(...) {
        return false;
    };
    static constexpr bool value = check(static_cast<DU*>(0));
};

// =================================
//       Simple grammar sugar
// =================================

// call "foo" for times
template <typename F, typename = typename std::enable_if<is_runnable<F>::value>::type>
void repeat(F&& foo, int times = 1) {
    for (int i = 0; i < times; ++i) {
        std::forward<F>(foo)();
    }
}


template <typename F, typename... Args>
void invoke(F&& call, Args&&... args) {
    static_assert(is_runnable<F, Args...>::value, "[HipeError]: Invoke non-runnable object !");
    call(std::forward<Args>(args)...);
}

template <typename Var>
void recyclePlus(Var& var, Var left_border, Var right_border) {
    var = (++var == right_border) ? left_border : var;
}

/**
 * Time wait for the runnable object
 * Use std::milli or std::micro or std::nano to fill template parameter
 */
template <typename Precision, typename F, typename... Args>
double timewait(F&& foo, Args&&... argv) {
    static_assert(is_runnable<F, Args...>::value, "[HipeError]: timewait for non-runnable object !");
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, Precision>(time_end - time_start).count();
}

/**
 * Time wait for the runnable object
 * And the precision is std::chrono::second
 */
template <typename F, typename... Args>
double timewait(F&& foo, Args&&... argv) {
    static_assert(is_runnable<F, Args...>::value, "[HipeError]: timewait for non-runnable object !");
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(time_end - time_start).count();
}

// ======================================
//            special format
// ======================================

/**
 * just like this:
 * =============
 * *   title   *
 * =============
 */
inline std::string title(const std::string& tar, int left_right_edge = 4) {
    static std::string ele1 = "=";
    static std::string ele2 = " ";
    static std::string ele3 = "*";

    std::string res;

    repeat([&] { res.append(ele1); }, left_right_edge * 2 + static_cast<int>(tar.size()));
    res.append("\n");

    res.append(ele3);
    repeat([&] { res.append(ele2); }, left_right_edge - static_cast<int>(ele3.size()));
    res.append(tar);
    repeat([&] { res.append(ele2); }, left_right_edge - static_cast<int>(ele3.size()));
    res.append(ele3);
    res.append("\n");

    repeat([&] { res.append(ele1); }, left_right_edge * 2 + static_cast<int>(tar.size()));
    return res;
}

/**
 * just like this
 * <[ something ]>
 */
inline std::string strong(const std::string& tar, int left_right_edge = 2) {
    static std::string ele1 = "<[";
    static std::string ele2 = "]>";

    std::string res;
    res.append(ele1);

    repeat([&] { res.append(" "); }, left_right_edge - static_cast<int>(ele1.size()));
    res.append(tar);
    repeat([&] { res.append(" "); }, left_right_edge - static_cast<int>(ele2.size()));

    res.append(ele2);
    return res;
}

inline std::string boundary(char element, int length = 10) {
    return std::string(length, element);
}

// ======================================
//             Basic module
// ======================================

// future container
template <typename T>
class Futures {
    std::vector<std::future<T>> futures;
    std::vector<T> results;

public:
    Futures()
      : futures(0)
      , results(0) {
    }

    // return results contained by the built-in vector
    std::vector<T>& get() {
        results.resize(futures.size());
        for (size_t i = 0; i < futures.size(); ++i) {
            results[i] = futures[i].get();
        }
        return results;
    }

    std::future<T>& operator[](size_t i) {
        return futures[i];
    }

    void push_back(std::future<T>&& future) {
        futures.push_back(std::move(future));
    }

    size_t size() {
        return futures.size();
    }

    // wait for all futures
    void wait() {
        for (size_t i = 0; i < futures.size(); ++i) {
            futures[i].wait();
        }
    }
};

// spin locker that use C++11 std::atomic_flag
class spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // HIPE_PAUSE();
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
    bool try_lock() {
        return !flag.test_and_set();
    }
};

// locker guard for spinlock
class spinlock_guard {
    spinlock* lck = nullptr;

public:
    explicit spinlock_guard(spinlock& locker) {
        lck = &locker;
        lck->lock();
    }
    ~spinlock_guard() {
        lck->unlock();
    }
};

/**
 * It is a safe task type that support saving different kinds of runnable object.
 * It allows the user to construct it by reference (lvalue or rvalue) and
 * internally construct a new runnable object by passing in a reference.
 */
class SafeTask {
    struct BaseExec {
        virtual void call() = 0;
        virtual ~BaseExec() = default;
    };

    template <typename F, typename T = typename std::decay<F>::type>
    struct GenericExec : BaseExec {
        T foo;
        GenericExec(F&& f)
          : foo(std::forward<F>(f)) {
            static_assert(!is_reference_wrapper<F>::value,
                          "[HipeError]: Use 'reference_wrapper' to save temporary variable is dangerous");
        }
        ~GenericExec() override = default;
        void call() override {
            foo();
        }
    };

public:
    SafeTask() = default;
    SafeTask(SafeTask&& other) = default;

    SafeTask(SafeTask&) = delete;
    SafeTask(const SafeTask&) = delete;
    SafeTask& operator=(const SafeTask&) = delete;

    ~SafeTask() = default;

    // construct a task
    template <typename F, typename = typename std::enable_if<is_runnable<F>::value>::type>
    SafeTask(F&& foo)
      : exe(new GenericExec<F>(std::forward<F>(foo))) {
    }

    // reset the task
    template <typename F, typename = typename std::enable_if<is_runnable<F>::value>::type>
    void reset(F&& foo) {
        exe.reset(new GenericExec<F>(std::forward<F>(foo)));
    }

    // the task was set
    bool is_set() {
        return static_cast<bool>(exe);
    }

    // override "="
    SafeTask& operator=(SafeTask&& tmp) {
        exe.reset(tmp.exe.release());
        return *this;
    }

    // runnable
    void operator()() {
        exe->call();
    }

private:
    std::unique_ptr<BaseExec> exe = nullptr;
};

/**
 * It is a quick Task that support saving different kinds of runnable object.
 * It allows user to construct it by reference(lvalue or rvalue). It will not contruct a new object inside it,
 * but extend the life of the runnable object through saving reference.
 */
class QuickTask {
    struct BaseExec {
        virtual void call() = 0;
        virtual ~BaseExec() = default;
    };

    template <typename F>
    struct GenericExec : BaseExec {
        F foo;
        GenericExec(F&& f)
          : foo(std::forward<F>(f)) {
        }
        ~GenericExec() override = default;
        void call() override {
            foo();
        }
    };

public:
    QuickTask() = default;
    QuickTask(QuickTask&& other) = default;

    QuickTask(QuickTask&) = delete;
    QuickTask(const QuickTask&) = delete;
    QuickTask& operator=(const QuickTask&) = delete;

    ~QuickTask() = default;

    // construct a task
    template <typename F, typename = typename std::enable_if<is_runnable<F>::value>::type>
    QuickTask(F&& foo)
      : exe(new GenericExec<F>(std::forward<F>(foo))) {
    }

    // reset the task
    template <typename F, typename = typename std::enable_if<is_runnable<F>::value>::type>
    void reset(F&& foo) {
        exe.reset(new GenericExec<F>(std::forward<F>(foo)));
    }

    // the task was set
    bool is_set() {
        return static_cast<bool>(exe);
    }

    // override "="
    QuickTask& operator=(QuickTask&& tmp) {
        exe.reset(tmp.exe.release());
        return *this;
    }

    // runnable
    void operator()() {
        exe->call();
    }

private:
    std::unique_ptr<BaseExec> exe = nullptr;
};

}  // namespace util

}  // namespace hipe
