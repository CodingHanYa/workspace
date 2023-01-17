#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <future>
#include <atomic>
#include <memory>
#include <iostream>


namespace hipe {

using HipeTimePoint = std::chrono::steady_clock::time_point;

template <typename T>
using HipeFutureVector = std::vector<std::future<T>>;

template <typename T>
using HipePromiseVector = std::vector<std::promise<T>>;

template <typename T>
using HipeFuture = std::future<T>;

template <typename T>
using HipePromise = std::promise<T>;


/**
 * util for hipe
*/
namespace util 
{

    HipeTimePoint tick() {
        return std::chrono::steady_clock::now();
    }

    void sleep_for_seconds(uint sec) {
        std::this_thread::sleep_for(std::chrono::seconds(sec));
    }

    void sleep_for_milli(uint milli) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milli));
    }

    void sleep_for_micro(uint micro) {
        std::this_thread::sleep_for(std::chrono::microseconds(micro));
    }

    void sleep_for_nano(uint nano) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
    }

    template <typename T>
    void print(T&& t) {
        std::cout<<std::forward<T>(t)<<std::endl;
    }

    template <typename T, typename... _Args>
    void print(T&& t, _Args&&... argv) {
        std::cout<<std::forward<T>(t);
        print(std::forward<_Args>(argv)...);
    }

    template <typename F>
    void repeat(F&& foo, int times = 1) {
        for (int i = 0; i < times; ++i) { foo(); }
    }

    /**
     * just like this: 
     * =============
     * *   title   *
     * =============
    */
    std::string title(std::string tar, size_t left_right_edge = 4) 
    {
        static std::string ele1 = "+";
        static std::string ele2 = " ";

        std::string res;

        repeat([&]{ res.append(ele1);}, left_right_edge * 2 + tar.size());
        res.append("\n");

        res.append(ele1);
        repeat([&]{ res.append(ele2); }, left_right_edge - ele1.size());
        res.append(tar);
        repeat([&]{ res.append(ele2); }, left_right_edge - ele1.size());
        res.append(ele1);
        res.append("\n");

        repeat([&]{ res.append(ele1);}, left_right_edge * 2 + tar.size());
        return res;
    }

    /**
     * just like this
     * <[ something ]>
    */
    std::string strong(std::string tar, size_t left_right_edge = 2) 
    {
        static std::string ele1 = "<[";
        static std::string ele2 = "]>";

        std::string res;
        res.append(ele1);

        repeat([&]{ res.append(" ");}, left_right_edge - ele1.size());
        res.append(tar);
        repeat([&]{ res.append(" ");}, left_right_edge - ele2.size());

        res.append(ele2);
        return res;

    }  

    std::string boundary(char element, size_t length = 10) {
        std::string res(length, element);
        return res;
    } 

    template <typename _Executable, typename... _Args>
    void invoke(_Executable& call, _Args&&... argv) {
        call(std::forward<_Args>(argv)...);
    }

    template <typename T, typename... _Args>
    void error(T&& err, _Args&&... argv) {
        print("[Hipe Error] ", std::forward<T>(err), std::forward<_Args>(argv)...);
        abort();
    }

    template <typename _Var>
    void recyclePlus(_Var& var, _Var left_border, _Var right_border) {
        var = (++var == right_border) ? left_border : var;
    }

    template <typename T>
    void futureBindPromise(HipeFutureVector<T>& futures, HipePromiseVector<T>&  promises) {
        for (int i = 0; i < std::min(futures.size(), promises.size()); ++i) {
            futures[i] = promises[i].get_future();
        }
    }

    template <typename T>
    void futureBindPromise(HipeFutureVector<T>& futures, HipePromiseVector<T>&  promises, int begin, int end) {
        for (int i = begin; i < end; ++i) {
            futures[i] = promises[i].get_future();
        }
    }

    template <typename T>
    void futureBindPromise(HipeFuture<T>& fut, HipePromise<T>& pro) {
        fut = pro.get_future();
    }

    /**
     * Time wait for the "call".
     * Use std::milli or std::micro or std::nano to fill template parameter
    */
    template <typename _Precision, typename F, typename... _Args>
    double timewait(F&& foo, _Args&&... argv) {
        auto time_start = std::chrono::steady_clock::now();
        foo(std::forward<_Args>(argv)...);
        auto time_end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, _Precision>(time_end-time_start).count();
    }

    /**
     * Time wait for the "call"
     * And the presition is std::chrono::second
    */
    template <typename F, typename... _Args>
    double timewait(F&& foo, _Args&&... argv) {
        auto time_start = std::chrono::steady_clock::now();
        foo(std::forward<_Args>(argv)...);
        auto time_end = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(time_end-time_start).count();
    }

    /**
     * Thread sync output stream. 
     * It can protect the output from multi thread competition.
    */
    class SyncStream
    {
        std::ostream& out_stream;
        std::recursive_mutex io_locker;

    public:
        SyncStream(std::ostream& out_stream = std::cout)
            : out_stream(out_stream) {
        }
        template <typename T>
        void print(T&& items)
        {   
            io_locker.lock();
            out_stream << std::forward<T>(items) << std::endl;
            io_locker.unlock();
        }
        template <typename T, typename... A>
        void print(T&& item, A&&... items)
        {   
            io_locker.lock();
            out_stream << std::forward<T>(item);
            this->print(std::forward<A>(items)...);
            io_locker.unlock();
        }
    };


    // spin locker that use C++11 std::atomic_flag 
    class spinlock 
    {
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
        void lock() {
            while (flag.test_and_set(std::memory_order_acquire));
        }
        void unlock() {
            flag.clear(std::memory_order_release);
        }
        bool try_lock() {
            return !flag.test_and_set();
        }
    };

    // locker guard for spinlock
    class spinlock_guard 
    {
        spinlock* lck = nullptr;
    public:
        spinlock_guard(spinlock& locker) {
            lck = &locker;
            lck->lock();
        }
        ~spinlock_guard() {
            lck->unlock();
        }
    };


    /**
     * Task that support different kinds of callable object.
     * It will alloc some heap space to save the task.
    */
    class Task 
    {      
        struct BaseExec {
            virtual void call() = 0;
            virtual ~BaseExec() {};
        };

        template <typename F>
        struct GenericExec: BaseExec { 
            F foo;
            GenericExec(F&& f): foo(std::forward<F>(f)) {}
            void call() override { foo();}
        };  

    public:

        Task() = default;
        ~Task() { delete ptr; }

        Task(Task&) = delete;
        Task(const Task&) = delete;
        Task& operator = (const Task&) = delete;

        template <typename F> 
        Task(F&& f): ptr(new GenericExec<F>(std::forward<F>(f))) {}
        Task(Task&& tmp): ptr(tmp.ptr) { tmp.ptr = nullptr; }
        
        template <typename Func> 
        void reset(Func&& f) {
            delete ptr;
            ptr = new GenericExec<Func>(std::forward<Func>(f));
        }

        bool is_setted() {
            return ptr != nullptr;
        }

        Task& operator = (Task&& tmp) {
            delete ptr;
            ptr = tmp.ptr;
            tmp.ptr = nullptr;
            return *this;
        }
        void operator()() { 
            ptr->call();
        }

    private:
        BaseExec* ptr = nullptr;       
    };


    /**
     * Block for adding task by betch
     * Use c array manage by unique_ptr
     * Notice that the element must override " = "
    */
    template <typename T>
    class Block 
    {
        size_t sz = 0;
        size_t end = 0;
        std::unique_ptr<T[]> blok = {nullptr};

    public:
        Block() = default;

        Block(Block&& other) 
            : blok(std::move(other.blok))
            , sz(other.sz)
            , end(other.end) {
        }

        Block(size_t sz)
            : blok(new T[sz]) 
            , sz(sz) {
        }

        virtual ~Block() {}

        T& operator [] (size_t idx) {
            return blok[idx];
        }

        // block capacity last
        size_t capacity() {
            return sz;   
        }

        // element number
        size_t element_numb() {
            return end;
        }

        // whether is able to contain nums of elements
        bool is_spare_for(size_t nums) {
            return (end + nums) <= sz;
        }

        // whether the block is full
        bool is_full() {
            return end == sz;
        }

        // add element
        void add(T&& tar) {
            blok[end++] = std::forward<T>(tar);
        }

        // fill element. Notice that the element must be able to copy !
        void fill(const T& tar) {
            while (end != sz) {
                blok[end++] = tar;
            }
        }

        // clean the block and delay free memory
        void clean() {
            end = 0;
        }

        // renew space for the block
        void reset(size_t new_sz) {
            blok.reset(new T[new_sz]);
            sz = new_sz;
            end = 0;
        }

        // release space
        void release() {
            blok.release();
            sz = 0;
            end = 0;
        }

        // just for inherit
        virtual void sort() {}

    };

};




}
