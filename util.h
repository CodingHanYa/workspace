#pragma once
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <future>
#include <atomic>
#include <cstddef>
#include <memory>
#include <iostream>

namespace hipe {

/**
 * util for hipe
*/
namespace util {

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
    inline std::string title(std::string tar, size_t left_right_edge = 4) 
    {
        static std::string ele1 = "=";
        static std::string ele2 = " ";
        static std::string ele3 = "*";

        std::string res;

        repeat([&]{ res.append(ele1);}, left_right_edge * 2 + tar.size());
        res.append("\n");

        res.append(ele3);
        repeat([&]{ res.append(ele2); }, left_right_edge - ele3.size());
        res.append(tar);
        repeat([&]{ res.append(ele2); }, left_right_edge - ele3.size());
        res.append(ele3);
        res.append("\n");

        repeat([&]{ res.append(ele1);}, left_right_edge * 2 + tar.size());
        return res;
    }

    /**
     * just like this
     * <[ something ]>
    */
    inline std::string strong(std::string tar, size_t left_right_edge = 2) 
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

    inline std::string boundary(char element, size_t length = 10) {
        return std::string(length, element);
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


    // future container
    template <typename T>
    class Futures
    {
        std::vector<std::future<T>> futures;
        std::vector<T>  results;

    public:
        Futures(): futures(0), results(0) {}

        // return results contained by the built-in vector
        std::vector<T>& get()
        {
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
        void wait() 
        {
            for (size_t i = 0; i < futures.size(); ++i) {
                futures[i].wait();
            }
        }


    };


    /**
     * Time wait for the runable object
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
     * Time wait for the runable object
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
     * Block for adding tasks in batch
     * You can regard it as a more convenient C arrays
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
        virtual ~Block() {}


        Block(Block&& other) 
            : blok(std::move(other.blok))
            , sz(other.sz)
            , end(other.end) {
        }

        Block(size_t sz)
            : blok(new T[sz]) 
            , sz(sz) {
        }

        Block(const Block& other) = delete;


        T& operator [] (size_t idx) {
            return blok[idx];
        }

        // block's capacity
        size_t capacity() {
            return sz;   
        }

        // element number
        size_t element_numb() {
            return end;
        }

        // whether have nums' space
        bool is_spare_for(size_t nums) {
            return (end + nums) <= sz;
        }

        // whether the block is full
        bool is_full() {
            return end == sz;
        }

        // add an element
        void add(T&& tar) {
            blok[end++] = std::forward<T>(tar);
        }

        // fill element. Notice that the element must can be copied !
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

        // release the heap space
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
