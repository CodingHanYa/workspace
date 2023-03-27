# 一个采用C++11编写的高性能、跨平台、简单易用且功能强大的C++并发库

**Hipe**是基于C++11编写的跨平台的、高性能的、简单易用且功能强大的并发库。其内置了三个职责分明的独立线程池：稳定线程池、动态线程池和均衡线程池，并提供了诸如任务包装器、计时器、支持重定向的同步输出流、C++11自旋锁等实用的工具。以下三种线程池分别称为Hipe-Steady、Hipe-Balance和Hipe-Dynamic。

优势: 
- **跨平台**：Hipe采用纯C++11编写，由C++的语言特性支持跨平台，构建工具采用跨平台的cmake。
- **高性能**：高效的任务同步模型以及线程间的负载均衡机制使得Hipe面对高速任务流时表现良好。
- **灵活性**：提供多种类型的线程池，使得Hipe能够应对不同的任务场景，提供高性能服务。
- **易用性**：Header only，且Hipe统一了三个线程池的大部分接口，降低了学习成本，同时在工具包中提供了简单高效的工具。
- **安全性**：Hipe的接口大都采用编译期检查类型（TMP技术）来提高接口的安全性，同时为一些错误使用提供文档说明。
- **稳定性**：Hipe在每一次改动后都会进行稳定性测试，在测试中针对不同线程池制定不同的测试策略。（详见Hipe/stability）

不足：
- 不适用于任务流控制场景（推荐使用[CGraph](https://github.com/ChunelFeng/CGraph))
- 不适用于高性能计算领域（推荐使用tbb等）


## 目录
- [我们从简单地提交一点任务开始](#我们从简单地提交一点任务开始)
- [批量获取返回值](#批量获取返回值)
- [批量提交任务](#批量提交任务)
- [动态调整线程数](#动态调整线程数)
- [三种线程池的底层机制](#三种线程池的底层机制)
    - [Hipe-SteadyThreadPond](#hipe-steadythreadpond)
    - [Hipe-BalancedThreadPond](#hipe-balancedthreadpond)
    - [Hipe-DynamicThreadPond](#hipe-dynamicthreadpond)
- [Performance-Benchmark](#performance-benchmark)
    - [加速比测试](#加速比测试)
    - [空任务测试](#空任务测试)
    - [其它任务测试](#其它任务测试)
    - [批量提交接口测试](#批量提交接口测试)
- [关于使用](#关于使用)
- [关于稳定性](#关于稳定性)
- [关于改进方向](#关于hipe接下来改进方向的提议)
- [关于错误使用案例](#关于错误使用案例)
- [鸣谢](#鸣谢)
- [联系我](#联系我)


## 我们从简单地提交一点任务开始
```C++

#include "hipe/hipe.h" 
using namespace hipe;

int main() {
    // SteadyThreadPond是Hipe的核心线程池类 
    SteadyThreadPond pond(8);

    // 提交任务，没有返回值。传入lambda表达式或者其它可调用类型
    // util::print()是Hipe提供的标准输出接口，让调用者可以像写python一样简单
    pond.submit([]{ util::print("HanYa said ", "hello world\n"); });

    // 带返回值的提交
    auto ret = pond.submitForReturn([]{ return 2023; });
    util::print("task return ", ret.get());

    // 主线程等待所有任务被执行
    pond.waitForTasks();

    // 主动关闭线程池，否则当线程池类被析构时由线程池自动调用
    pond.close();
}


```

## 批量获取返回值

```C++

#include "hipe/hipe.h" 
using namespace hipe;

int main() 
{
    // 动态线程池
    DynamicThreadPond pond(8);
    HipeFutures<int> futures;

    for (int i = 0; i < 5; ++i) {
        auto ret = pond.submitForReturn([i]{ return i+1; }); 
        futures.push_back(std::move(ret));
    }

    // 等待所有任务被执行
    futures.wait();

    // 获取所有异步任务结果
    auto rets = futures.get();

    for (int i = 0; i < 5; ++i) {
        util::print("return ", rets[i]);
    }
}

```

## 批量提交任务

```C++

#include "hipe/hipe.h" 
#include <vector>
using namespace hipe;

int main() 
{
    // 均衡线程池，参数2为线程池缓冲任务容量
    BalancedThreadPond pond(8, 800); 
    
    // 任务容器
    std::vector<std::function<void()>> tasks;
    
    int task_numb = 5;
    for (int i = 0; i < task_numb; ++i) {
        tasks.emplace_back([]{ /* empty task */ });
    }

    // 批量提交任务
    pond.submitInBatch(tasks, task_numb);
    pond.waitForTasks();

}

```
## 动态调整线程数

```C++
#include "hipe/hipe.h" 
using namespace hipe;

int main() {

    DynamicThreadPond pond(8);

    // 添加线程
    pond.addThreads(8);

    // 等待线程进入工作循环中
    pond.waitForThreads();

    // 获取已在运行的线程数
    util::print("Now the the number of running thread is: ", pond.getRunningThreadNumb());

    // 调整线程数为零。与调用: pond.delThreads(16)的作用相同;
    pond.adjustThreads(0); 

    // 当调用调整线程数的接口时，线程数的期望值会立刻改变
    util::print("Expect the number of thread now is: ", pond.getExpectThreadNumb());

    // 但此时线程可能还在删除过程中，因此此时运行中的线程的数量不会是零，会逐渐减少。
    util::print("Get running thread count again: ", pond.getRunningThreadNumb());

    // 等待线程全部关闭
    pond.waitForThreads();

    // 回收关闭的线程，否则当线程池析构时自动回收所有关闭的线程
    pond.joinDeadThreads();

    // 删除所有线程并回收所有死亡线程（线程池析构时也会自动调用close）
    pond.close();
}

```

更多接口的调用请大家阅读`hipe/interfaces/`，里面有几乎全部的接口测试，并且每一个函数调用都有较为详细的注释。



## 三种线程池的底层机制

### Hipe-SteadyThreadPond

Hipe-Steady是Hipe提供的稳定的、具有固定线程数的线程池。支持批量提交任务和批量执行任务、支持有界任务队列和无界任务队列、支持池中线程的**任务窃取机制**。任务溢出时支持**注册回调**并执行或者**抛出异常**。

Hipe-Steady所调用的线程类`DqThread`为每个线程都分配了公开任务队列、缓冲任务队列和控制线程的同步变量（thread-local机制），尽量降低**乒乓缓存**和**线程同步**对线程池性能的影响。工作线程通过队列替换**批量下载**公开队列的任务到缓冲队列中执行。生产线程则通过公开任务队列为工作线程**分配任务**（采用了一种优于轮询的**负载均衡**机制）。公开队列和缓冲队列（或说私有队列）替换的机制进行**读写分离**，再通过加**轻锁**（C++11原子量实现的自旋锁）的方式极大地提高了线程池的性能。

由于其底层的实现机制，Hipe-Steady适用于**稳定的**（避免超时任务阻塞线程）、**任务量大**（任务传递的优势得以体现）的任务流。也可以说Hipe-Steady适合作为核心线程池（能够处理基准任务并长时间运行），而当可以**定制容量**的Hipe-Steady面临任务数量超过设定值时 —— 即**任务溢出**时，我们可以通过定制的**回调函数**拉取出溢出的任务，并把这些任务推到我们的动态线程池DynamicThreadPond中。在这个情景中，DynamicThreadPond或许可以被叫做CacheThreadPond缓冲线程池。关于二者之间如何协调运作，大家可以阅读`Hipe/demo/demo1.cpp`.在这个demo中我们展示了如何把DynamicThreadPond用作Hipe-Steady的缓冲池。



**框架图**

![Hipe-Steady.jpg](https://s2.loli.net/2023/02/05/ky6OcLd1MrjzU84.jpg)

### Hipe-BalancedThreadPond

Hipe-Balance对比Hipe-Steady除了对其所使用的线程类做了简化之外，其余的机制包括线程间负载均衡和任务溢出机制等都是相同的。提供的接口也是相同的。同时，与Hipe-Steady面向批量任务的思想不同，Hipe-Balance采用的是与Hipe-Dynamic相同的**面向单个任务**的思想，即每次只获取一个任务并执行。这也使得二者工作线程的工作方式略有不同。

决定Hipe-Balanced和Hipe-Steay之间机制差异的根本原因在于其所采用的线程类的不同。前者采用的是`Oqthread`，译为**单队列线程**。内置了单条任务队列，主线程采用一种优于轮询的负载均衡机制向线程类内部的任务队列分发任务，工作线程直接查询该任务队列并获取任务。后者采用的是`DqThread`，译为**双队列线程**，采用的是队列交换的机制。

相比于Hipe-Steady，Hipe-Balanced在异步线程与主线程之间**竞争次数较多**的时候性能会有所下降，同时其**批量提交**接口的表现也会有所下降，甚至有可能低于其提交单个任务的接口（具体还要考虑任务类型等许多复杂的因素）。但是由于线程类中只有一条任务队列，因此所有任务都是可以被**窃取**的。这也导致Hipe-Balance在面对**不稳定的任务流**时（可能会有超时任务）具有更好的表现。



**框架图**

![Hipe-Balance.jpg](https://s2.loli.net/2023/02/05/xlpTYmAjCzXnUGF.jpg)


### Hipe-DynamicThreadPond

Hipe-Dynamic是Hipe提供的动态的、能够**扩缩容**的线程池。支持批量提交任务、支持线程池吞吐任务速率监测、支持无界队列。当没有任务时所有线程会被自动挂起（阻塞等待条件变量的通知），较为节约CPU资源。

Hipe-Dynamic采用的是**多线程竞争单任务队列**的模型。该任务队列是无界的，能够容蓄大量的任务，直至系统资源耗尽。由于Hipe-Dynamic管理的线程没有私有的任务队列且面向单个任务，因此能够被灵活地调度。同时，为了能动态调节线程数，Hipe-Dynamic还提供了能监测线程池执行速率的接口，其使用实例在`Hipe/demo/demo2`。

由于Hipe-Dynamic的接口较为简单，如果需要了解更多接口的调用，可以阅读接口测试文件`Hipe/interfaces/`或者`Hipe/demo/demo2`。



**框架图**

![Hipe-Dynamic.jpg](https://s2.loli.net/2023/02/05/lYnbNxmREkzKAH7.jpg)



## Performance BenchMark

[bshoshany](https://github.com/bshoshany)/**[thread-pool](https://github.com/bshoshany/thread-pool)** （以下简称BS）是在GitHub上开源的已收获了**1k+stars** 的C++线程池，采用C++17编写，具有轻量，高效的特点。我们通过**加速比测试和空任务测试**，对比BS和Hipe的性能。实际上BS的底层机制与Hipe-Dynamic相似，都是多线程竞争一条任务队列，并且在没有任务时被条件变量阻塞。同时我们也通过其它任务测试和批量接口测试，对比Hipe-Steady和Hipe-Balance的性能差异。

测试机器：16核_ubuntu20.04 （以下测试都开启**O2优化**）

### 加速比测试

测试原理： 通过执行**计算密集型**的任务，与单线程进行对比，进而算出线程池的加速比。每次测试都会重复5遍并取平均值。

```C++
// ================================================
//    computation intensive task(计算密集型任务)
// ================================================

int vec_size = 4096;
int vec_nums = 2048;
std::vector<std::vector<double>> results(vec_nums, std::vector<double>(vec_size));

void computation_intensive_task() {
    for (int i = 0; i < vec_nums; ++i) {
        for (int j = 0; j < vec_size; ++j) {
            results[i][j] = std::log(std::sqrt(std::exp(std::sin(i) + std::cos(j))));
        }
    }
}
```

以下是执行结果。为了结果更准确，我们每次测试都只测一个线程池（或单线程），然后等待机器散热。每次测试中间隔了30~40秒。

```

=======================================================
*           Test Single-thread Performance            *
=======================================================

threads: 1  | task-type: compute mode | task-numb: 4  | time-cost-per-task: 201.92240(ms)

=======================================================
*             Test C++(17) Thread-Pool BS             *
=======================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 54.28629(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 20.71979(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 22.93107(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 25.67357(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 25.85536(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 22.94270(ms)

=================================================================
*             Test C++(11) Thread-Pool Hipe-Dynamic             *
=================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 54.00041(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 20.91941(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 22.59373(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 26.15185(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 25.70997(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 23.45609(ms)

================================================================
*             Test C++(11) Thread-Pool Hipe-Steady             *
================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 79.93079(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 20.50024(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 24.34773(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 27.41396(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 28.06915(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 22.93314(ms)

=================================================================
*             Test C++(11) Thread-Pool Hipe-Balance             *
=================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 83.34004(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 20.42817(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 24.87741(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 27.32333(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 27.96282(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 22.91393(ms)
```

计算最佳加速比

```
公式: 
	单线程的平均任务耗时/多线程的最小平均任务耗时（四舍五入）
结果: 
    BS: 9.75
    Hipe-Dynamic: 9.65
    Hipe-Steady: 9.85
    Hipe-Balance: 9.88
```

结果分析：四个线程池在加速比方面的性能十分相近，这可能是因为任务数较少，执行任务的时间较长且在执行过程中不涉及缓存一致性等其它因素。因此线程池内部的编写细节对整体加速比的影响不大。

### 空任务测试

测试原理： 通过提交大量的空任务到线程池中，对比两种线程池处理空任务的能力，其主要影响因素为**线程同步任务**以及工作线程循环过程中的**其它开销**。


```

===================================
*   Test C++(17) Thread Pool BS   *
===================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00112(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01055(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.09404(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.94548(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.49177(s)

=============================================
*   Test C++(11) Thread Pool Hipe-Dynamic   *
=============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00149(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01063(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.09377(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.94485(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.42167(s)

============================================
*   Test C++(11) Thread Pool Hipe-Steady   *
============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00129(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00020(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00211(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.03234(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.21916(s)

=============================================
*   Test C++(11) Thread Pool Hipe-Balance   *
=============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00005(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00022(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00228(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.02098(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.20107(s)

=============================================
*              End of the test              *
=============================================

```

结果分析： 可以看到在处理空任务这一方面Hipe-Steady和Hipe-Balance具有**巨大的优势**，在处理**1000000**个空任务时性能是BS和Hipe-Dynamic的**45倍左右**。而如果Hipe-steady采用批量提交的接口的话，能够达到约**50倍左右**的性能提升。

### 其它任务测试

测试原理：我们采用的是一个**内存密集型任务**（只在任务中申请一个vector），同时将线程数限制在较少的**4条**来对比Hipe-Steay和Hipe-Balance的性能。用于证明在**某种情况**下，例如工作线程的工作速度与主线程分配任务给该线程的速度相等，主线程与工作线程形成较强的竞争的情况下，Hipe-Steady对比Hipe-Balance更加卓越。其中的关键就是Hipe-Steady通过队列交换实现了部分读写分离，减少了一部分**潜在的**竞争。（每次测试20次并取平均值）

```
=============================================
*   Hipe-Steady Run Memory Intensive Task   *
=============================================
thread-numb: 4  | task-numb: 1000000  | test-times: 20 | mean-time-cost: 0.17990(s)

==============================================
*   Hipe-Balance Run Memory Intensive Task   *
==============================================
thread-numb: 4  | task-numb: 1000000  | test-times: 20 | mean-time-cost: 0.21142(s)

```

因此，如果你能确保任务的执行时间是十分稳定的，不存在超时任务阻塞线程的情况。那么你有理由采用Hipe-Steady来提供更高效的服务的。但是如果你担心超时任务阻塞线程的话，那么我更推荐采用Hipe-Balance来作为核心线程池提供服务。具体还要应用到实际中进行调试。

### 批量提交接口测试

注意：单次批量提交的任务数为**10个**。每次测试之间留有**30秒以上**的时间间隔。

**<<测试1>>**

测试原理：调用Hipe-Steady和Hipe-Balance的**批量提交接口**提交大量的**空任务**，同时**不开启**任务缓冲区限制机制，即采用无界队列。通过结果对比展示**延长单次加锁时间**对两个线程池性能的影响。需要注意，如果我们开启了任务缓冲区限制机制，即采用了有界队列，则批量提交时两个线程池采用的是与**单次提交**相同的**加锁策略**。即每提交一个任务到队列中时加一次锁。

```
=============================================================
*   Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)   *
=============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00602(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00016(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00163(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.01128(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.17955(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 1.34524(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 11.54472(s)

==============================================================
*   Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)   *
==============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00573(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00026(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00135(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.01880(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.18571(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 1.71107(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 15.39781(s)

```

**<<测试2>>**

测试原理：调用Hipe-Steady和Hipe-Balance的**批量提交接口**提交大量的**空任务**，同时**开启**任务缓冲区限制机制，即采用有界队列。通过结果对比展示增强主线程与工作线程间**竞争**对两个线程池性能的影响。此时加锁策略为每次提交一次任务就加一次锁。

```
=============================================================
*   Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)   *
=============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00038(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00019(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00143(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.01231(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.23335(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 1.45892(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 13.32421(s)

==============================================================
*   Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)   *
==============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00006(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00047(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00664(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.01784(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.18197(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 1.43280(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 16.73147(s)

```

## 关于使用

```shell

# 简单使用
g++ -I Hipe/include xxx.cc && ./a.out

# 运行已有实例（以demo为例）
# 在根目录下: 
cmake -B build 
cd build/demo
make demo1
./demo1

# 在我们的cmake中，所有可执行文件的名字都与文件名(除后缀)相同。当然，嵌套的可执行文件还要加上相对路径如:
# xxx/file.cpp，此时CMakeLists.txt与xxx同级，那么生成的可执行文件为: xxx_file(.exe)
```

## 关于错误使用案例

1. 将`waitForTasks()`接口作为**同一个线程池**的任务来执行，会导致执行该任务的线程永远阻塞。例如：

```C++
SteadyThreadPond pond(8);
pond.submut([&]{pond.waitForTasks();});
```

2. 主线程和异步线程调用相同的线程池接口，导致数据竞争，引发程序中断。需要注意！Hipe的所有接口都在实现时均不考虑线程之间的竞争问题。
可以异步调用的接口，如动态线程池调整线程数的接口等已在`Hipe/demo/demo2.cpp`中演示

3. 通过`std::ref`传入`std::reference_wrapper`保存的可执行对象。通过**引用包装器**包装可执行对象的行为是被禁止的，因为引用包装器内部保存的是构造时传入参数的指针，


## 关于Hipe接下来改进方向的提议

1. 通过优化锁来提高三个线程池的性能
2. 通过优化任务来提高线程池的性能（采用栈空间构造任务，当然是否有真正的提升还需要测试）
3. 在基本不影响原来性能的前提下，为Hipe-Balance和Hipe-Steady添加扩缩容机制。（私以为可以采用util::block来预先设置最大容量，再在内部做线程数调整）
4. 为Hipe提供执行的日志系统，用于监测Hipe执行的总体情况
5. 如果采用C++14以上的C++版本能做到更高的性能的话，提供相应的版本。

最后，本人学识有限，目前也是在不断地学习中。如果您发现了Hipe的不足之处，请在issue中提出或者提PR，我非常欢迎并在此提前感谢各位。世事无常，也许有一天Hipe能在C++并发领域成长为真正的佼佼者，解决众多开发者的并发难题！对此我十分期待。



## 鸣谢

Hipe参与贡献者

一直支持我的女朋友小江和我的父母、姐姐。

《C++并发编程实战》

《Java并发编程的艺术》

BS的贡献者

小林技术交流群中的各位大佬

阿里大佬 Chunel 

## 联系我

QQ邮箱：1848395727@qq.com

