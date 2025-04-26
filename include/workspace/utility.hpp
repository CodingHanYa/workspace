#pragma once
#include <cstdlib>
#include <cstddef>
#include <utility>
#include <deque>
#include <functional>
#include <future>
#include <type_traits>
#include <vector>

namespace wsp {
namespace details {

// type trait
#if defined(_MSC_VER)
#if _MSVC_LANG >= 201703L
template <typename F, typename... Args>
using result_of_t = std::invoke_result_t<F, Args...>;
#else
template <typename F, typename... Args>
using result_of_t = typename std::result_of<F(Args...)>::type;
#endif
#else
#if __cplusplus >= 201703L
template <typename F, typename... Args>
using result_of_t = std::invoke_result_t<F, Args...>;
#else
template <typename F, typename... Args>
using result_of_t = typename std::result_of<F(Args...)>::type;
#endif
#endif

// type trait
struct normal {};    // normal task (for type inference)
struct urgent {};    // urgent task (for type inference)
struct sequence {};  // sequence tasks (for type inference)

// function_: try to avoid heap allocation

template<typename Signature, size_t InlineSize = 64 - sizeof(void*)>
class function_;

template<typename T>
struct is_function_ : std::false_type {};

template<typename R, size_t N>
struct is_function_<function_<R, N>> : std::true_type {};

template<typename R, typename... Args, size_t InlineSize>
class function_<R(Args...), InlineSize> {
private:
    struct callable_base {
        virtual R invoke(Args&&...) = 0;
        virtual void move_into(void* buffer) = 0;
        virtual void clone_into(void* buffer) const = 0;
        virtual ~callable_base() = default;
    };

    template<typename F>
    struct callable_impl : callable_base {
        F f;

        template<typename U>
        callable_impl(U&& fn) : f(std::forward<U>(fn)) {}

        R invoke(Args&&... args) override {
            return f(std::forward<Args>(args)...);
        }
        void move_into(void* buffer) override {
            new (buffer) callable_impl(std::move(f));
        }
        void clone_into(void* buffer) const override {
            new (buffer) callable_impl(f);
        }
    };

    template<typename F>
    struct heap_callable_impl : callable_base {
        F* pf;

        heap_callable_impl() : pf(nullptr) {} ;
        template<typename U>
        heap_callable_impl(U&& fn) : pf(new F(std::forward<U>(fn))) {}

        heap_callable_impl(const heap_callable_impl&) = delete;
        heap_callable_impl& operator=(const heap_callable_impl&) = delete;

        heap_callable_impl(heap_callable_impl&&) = default;
        heap_callable_impl& operator=(heap_callable_impl&&) = default;

        ~heap_callable_impl() {  delete pf; }

        R invoke(Args&&... args) override {
            return (*pf)(std::forward<Args>(args)...);
        }
        void move_into(void* buffer) override {
            auto pc = new (buffer) heap_callable_impl();
            pc->pf = pf;
            pf = nullptr;
        }
        void clone_into(void* buffer) const override {
            new (buffer) heap_callable_impl(*pf);
        }
    };

public:
    static constexpr size_t inline_size = InlineSize;

    function_() = default;
    function_(std::nullptr_t) {}
    function_(const function_& other) {
        if (other.callable) {
            other.callable->clone_into(buffer);
            callable = reinterpret_cast<callable_base*>(&buffer);
        }
    }
    function_(function_&& other) noexcept {
        if (other.callable) {
            other.callable->move_into(buffer);
            callable = reinterpret_cast<callable_base*>(&buffer);
            other.callable = nullptr;
        }
    }
    template<typename F,
        typename T = typename std::decay<F>::type,
        typename std::enable_if<!is_function_<T>::value, int>::type = 0,
        typename std::enable_if<(sizeof(callable_impl<T>) > InlineSize), int>::type = 0>
    function_(F&& f) {
        new (buffer) heap_callable_impl<T>(std::forward<F>(f));
        callable = reinterpret_cast<callable_base*>(&buffer);
    }

    template<typename F,
        typename T = typename std::decay<F>::type,
        typename std::enable_if<!is_function_<T>::value, int>::type = 0,
        typename std::enable_if<(sizeof(callable_impl<T>) <= InlineSize), int>::type = 0>
    function_(F&& f) {
        new (buffer) callable_impl<T>(std::forward<F>(f));
        callable = reinterpret_cast<callable_base*>(&buffer);
    }

    function_& operator=(const function_& other) {
        if (this != &other) {
            reset();
            if (other.callable) {
                other.callable->clone_into(buffer);
                callable = reinterpret_cast<callable_base*>(&buffer);
            }
        }
        return *this;
    }

    function_& operator=(function_&& other) noexcept {
        if (this != &other) {
            reset();
            if (other.callable) {
                other.callable->move_into(buffer);
                callable = reinterpret_cast<callable_base*>(&buffer);
                other.callable = nullptr;
            }
        }
        return *this;
    }

    ~function_() {
        reset();
    }

    void reset() {
        if (callable) {
            callable->~callable_base();
            callable = nullptr;
        }
    }

    explicit operator bool() const {
        return callable != nullptr;
    }

    R operator()(Args... args) const {
        if (!callable)
            throw std::bad_function_call();
        return callable->invoke(std::forward<Args>(args)...);
    }

private:
    alignas(std::max_align_t) char buffer[InlineSize];
    callable_base* callable = nullptr;
};


// using task_t = std::function<void()>;
using task_t = function_<void()>;

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
        for (auto& each : futs) {
            each.wait();
        }
    }
    size_t size() {
        return futs.size();
    }
    /**
     * @brief get set of result
     * @return std::vector<T>
     */
    std::vector<T> get() {
        std::vector<T> res;
        for (auto& each : futs) {
            res.emplace_back(each.get());
        }
        return res;
    }

    iterator end() {
        return futs.end();
    }

    iterator begin() {
        return futs.begin();
    }

    void add_back(std::future<T>&& fut) {
        futs.emplace_back(std::move(fut));
    }

    void add_front(std::future<T>&& fut) {
        futs.emplace_front(std::move(fut));
    }

    void for_each(std::function<void(std::future<T>&)> deal) {
        for (auto& each : futs) {
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
    auto operator[](size_t idx) -> std::future<T>& {
        return futs[idx];
    }
};

}  // namespace details
}  // namespace wsp
