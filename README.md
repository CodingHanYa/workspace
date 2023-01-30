# 一个基于C++11的高性能的、跨平台的、简单易用的线程池框架（thread pool framework）

**Hipe**是基于C++11编写的跨平台的、高性能的、简单易用且功能强大的线程池框架（thread pool framework），每秒能够空跑**上百万**的任务。其内置了三个职责分明的独立线程池：SteadyThreadPond稳定线程池、DynamicThreadPond动态线程池和BalancedThreadPond均衡线程池，并提供了诸如任务包装器、计时器、支持重定向的同步输出流、C++11自旋锁等实用的工具。使用者可以根据业务类型单独使用或者结合使用三种线程池来提供高并发服务。以下三种线程池分别称为Hipe-Steady、Hipe-Balance和Hipe-Dynamic。

bilibili源码剖析视频：https://space.bilibili.com/499976060 （根据源码迭代持续更新）

## demo1-简单地提交一点任务

```C++

#include "./Hipe/hipe.h" 
using namespace hipe;

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

```


## demo2-批量获取返回值

```C++

#include "./Hipe/hipe.h" 
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



更多接口的调用请大家阅读`hipe/interface_test/`，里面有全部的接口测试，并且每一个函数调用都有较为详细的注释。

由于BalancedThreadPond和SteadyThreadPond的接口一致，二者都继承自一个统一接口的类FixedThreadPond（固定线程数的线程池基类，定义了负载均衡的算法和任务溢出机制等），因此不再展示。



## Hipe-SteadyThreadPond
Hipe-Steady是Hipe提供的稳定的、具有固定线程数的线程池。支持批量提交任务和批量执行任务、支持有界任务队列和无界任务队列、支持池中线程的**任务窃取机制**。任务溢出时支持**注册回调**并执行或者**抛出异常**。

Hipe-Steady所调用的线程类`DqThread`为每个线程都分配了公开任务队列、缓冲任务队列和控制线程的同步变量（thread-local机制），尽量降低**乒乓缓存**和**线程同步**对线程池性能的影响。工作线程通过队列替换**批量下载**公开队列的任务到缓冲队列中执行。生产线程则通过公开任务队列为工作线程**分配任务**（采用了一种优于轮询的**负载均衡**机制）。通过公开队列和缓冲队列（或说私有队列）替换的机制进行**读写分离**，再通过加**轻锁**（C++11原子量实现的自旋锁）的方式极大地提高了线程池的性能。

由于其底层的实现机制，Hipe-Steady适用于**稳定的**（避免超时任务阻塞线程）、**任务量大**（任务传递的优势得以体现）的任务流。也可以说Hipe-Steady适合作为核心线程池（能够处理基准任务并长时间运行），而当可以**定制容量**的Hipe-Steady面临任务数量超过设定值时 —— 即**任务溢出**时，我们可以通过定制的**回调函数**拉取出溢出的任务，并把这些任务推到我们的动态线程池DynamicThreadPond中。在这个情景中，DynamicThreadPond或许可以被叫做CacheThreadPond缓冲线程池。关于二者之间如何协调运作，大家可以阅读`Hipe/demo/demo1`.在这个demo中我们展示了如何把DynamicThreadPond用作Hipe-Steady的缓冲池。

## Hipe-BalancedThreadPond

Hipe-Balance对比Hipe-Steady除了对其所使用的线程类做了简化之外，其余的机制包括线程间负载均衡和任务溢出机制等都是相同的。提供的接口也是相同的。同时，与Hipe-Steady面向批量任务的思想不同，Hipe-Balance采用的是与Hipe-Dynamic相同的**面向单个任务**的思想，即每次只获取一个任务并执行。这也使得二者工作线程的工作方式略有不同。

决定Hipe-Balanced和Hipe-Steay之间机制差异的根本原因在于其所采用的线程类的不同。前者采用的是`Oqthread`，译为**单队列线程**。内置了单条任务队列，主线程采用一种优于轮询的负载均衡机制向线程类内部的任务队列分发任务，工作线程直接查询该任务队列并获取任务。后者采用的是`DqThread`，译为双队列线程，采用的是队列交换的机制。

相比于Hipe-Steady，Hipe-Balanced在异步线程与主线程之间**竞争次数较多**的时候性能会有所下降，同时其**批量提交**接口的表现也会有所下降，甚至会低于其提交单个任务的接口（具体还要考虑任务类型等许多复杂的因素）。但是由于线程类中只有一条任务队列，因此所有任务都是可以被窃取的。这也导致Hipe-Balance在面对**不稳定的任务流**时（可能会有超时任务）具有更好的表现。


## Hipe-DynamicThreadPond

Hipe-Dynamic是Hipe提供的动态的、能够**扩缩容**的线程池。支持批量提交任务、支持线程池吞吐任务速率监测、支持无界队列。当没有任务时所有线程会被自动挂起（阻塞等待条件变量的通知），较为节约CPU资源。

Hipe-Dynamic采用的是**多线程竞争单任务队列**的模型。该任务队列是无界的，能够容蓄大量的任务，直至系统资源耗尽。由于Hipe-Dynamic管理的线程没有私有的任务队列且面向单个任务，因此能够被灵活地调度。同时，为了能动态调节线程数，Hipe-Dynamic还提供了能监测线程池执行速率的接口，其使用实例在`Hipe/demo/demo2`。

由于Hipe-Dynamic的接口较为简单，如果需要了解更多接口的调用，可以阅读接口测试文件`Hipe/interface_test/`或者`Hipe/demo/demo2`。



## Performance BenchMark

[bshoshany](https://github.com/bshoshany)/**[thread-pool](https://github.com/bshoshany/thread-pool)** （以下简称BS）是在GitHub上开源的已收获了**1k+stars** 的C++线程池，采用C++17编写，具有轻量，高效的特点。我们通过**加速比测试和空任务测试**，对比BS和Hipe的性能。实际上BS的底层机制与Hipe-Dynamic相似，都是多线程竞争一条任务队列，并且在没有任务时被条件变量阻塞。同时我们也通过其它任务测试和批量接口测试，对比Hipe-Steady和Hipe-Balance的性能差异。

测试机器：16核_ubuntu20.04

### 加速比测试

测试原理： 通过执行**计算密集型**的任务，与单线程进行对比，进而算出线程池的加速比。每次测试都会重复5遍并取平均值。

```C++
// ================================================
// 		computation intensive task(计算密集型任务)
// ================================================

uint vec_size = 4096;
uint vec_nums = 2048;
std::vector<std::vector<double>> results(vec_nums, std::vector<double>(vec_size));

void computation_intensive_task() {
    for (int i = 0; i < vec_nums; ++i) {
        for (size_t j = 0; j < vec_size; ++j) {
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

threads: 1  | task-type: compute mode | task-numb: 4  | time-cost-per-task: 341.69838(ms)

=======================================================
*             Test C++(17) Thread-Pool BS             *
=======================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 90.64565(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 38.15237(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 41.32091(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 43.71364(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 43.93374(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 38.54905(ms)

=================================================================
*             Test C++(11) Thread-Pool Hipe-Dynamic             *
=================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 91.25911(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 37.19642(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 41.11306(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 43.36172(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 43.16378(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 39.30077(ms)

================================================================
*             Test C++(11) Thread-Pool Hipe-Steady             *
================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 136.05910(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 36.30970(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 44.45373(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 47.25544(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 47.33378(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 40.40722(ms)

=================================================================
*             Test C++(11) Thread-Pool Hipe-Balance             *
=================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 136.24264(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 35.90849(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 43.64995(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 46.85115(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 47.46300(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 39.93729(ms)

```

计算最佳加速比

```
公式: 
	单线程的平均任务耗时 / 多线程的最小平均任务耗时
结果: 
    BS: 8.96
    Hipe-Dynamic: 9.19
    Hipe-Steady: 9.41
    Hipe-Balance: 9.52
```

结果分析：BS和Hipe-Dynamic性能接近，本质是二者都采用了多线程竞争单任务队列的模型。而Hipe-Steady和Hipe-Balance的加速比都略高于前者，本质是他们都采用了多任务队列的模型。

### 空任务测试

测试原理： 通过提交大量的空任务到线程池中，对比两种线程池处理空任务的能力，其主要影响因素为**线程同步任务**以及工作线程循环过程中的**其它开销**。


```

===================================
*   Test C++(17) Thread Pool BS   *
===================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00126(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01104(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.09691(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.98304(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.84680(s)

=============================================
*   Test C++(11) Thread Pool Hipe-Dynamic   *
=============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00131(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01104(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.09814(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.97543(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.75777(s)

============================================
*   Test C++(11) Thread Pool Hipe-Steady   *
============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00005(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00049(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00349(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.02929(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.28559(s)

=============================================
*   Test C++(11) Thread Pool Hipe-Balance   *
=============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00006(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00053(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00364(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.03205(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.31110(s)

=============================================
*              End of the test              *
=============================================

```

结果分析： 可以看到在处理空任务这一方面Hipe-Steady和Hipe-Balance具有**巨大的优势**，在处理**1000000**个空任务时性能是BS和Hipe-Dynamic的**20倍以上**。而如果Hipe-steady采用批量提交的接口的话，能够达到约**30倍以上**的性能提升。

### 其它任务测试

测试原理：我们采用的是一个**内存密集型任务**（只在任务中申请一个vector），同时将线程数限制在较少的**4条**来对比Hipe-Steay和Hipe-Balance的性能。用于证明在某种情况下，例如工作线程的工作速度与主线程分配任务给该线程的速度相等，主线程与工作线程形成较强的竞争的情况下，Hipe-Steady对比Hipe-Balance更加卓越。其中的关键就是Hipe-Steady通过队列交换实现了部分读写分离，减少了一部分**潜在的**竞争。（测试20次取平均值）

```
=============================================
*   Hipe-Steady Run Memory Intensive Task   *
=============================================
thread-numb: 4  | task-numb: 1000000  | test-times: 20 | mean-time-cost: 0.32058(s)

==============================================
*   Hipe-Balance Run Memory Intensive Task   *
==============================================
thread-numb: 4  | task-numb: 1000000  | test-times: 20 | mean-time-cost: 0.39317(s)
```

因此，如果你能确保任务的执行时间是十分稳定的，不存在超时任务阻塞线程的情况。那么你有理由采用Hipe-Steady来提供更高效的服务的。但是如果你担心超时任务阻塞线程的话，那么我更推荐采用Hipe-Balance来作为核心线程池提供服务。具体还要应用到实际中进行调试。



### 批量提交接口测试

注意：单次批量提交的任务数为**10个**。每次测试之间留有30秒以上的时间间隔。



**<<测试1>>**

测试原理：调用Hipe-Steady和Hipe-Balance的**批量提交接口**提交大量的**空任务**，同时**不开启**任务缓冲区限制机制，即采用无界队列。通过结果对比展示**延长单次加锁时间**对两个线程池性能的影响。需要注意，如果我们开启了任务缓冲区限制机制，即采用了有界队列，则批量提交时两个线程池采用的是与单次提交相同的**加锁策略**。即每提交一个任务到队列中时加一次锁。

```
=============================================================
*   Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)   *
=============================================================
threads: 16  | task-type: empty task | task-numb: 100       | time-cost: 0.00004(s)
threads: 16  | task-type: empty task | task-numb: 1000      | time-cost: 0.00024(s)
threads: 16  | task-type: empty task | task-numb: 10000     | time-cost: 0.00237(s)
threads: 16  | task-type: empty task | task-numb: 100000    | time-cost: 0.04381(s)
threads: 16  | task-type: empty task | task-numb: 1000000   | time-cost: 0.22448(s)
threads: 16  | task-type: empty task | task-numb: 10000000  | time-cost: 2.04291(s)
threads: 16  | task-type: empty task | task-numb: 100000000 | time-cost: 23.77099(s)

==============================================================
*   Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)   *
==============================================================
threads: 16  | task-type: empty task | task-numb: 100       | time-cost: 0.00007(s)
threads: 16  | task-type: empty task | task-numb: 1000      | time-cost: 0.00043(s)
threads: 16  | task-type: empty task | task-numb: 10000     | time-cost: 0.00378(s)
threads: 16  | task-type: empty task | task-numb: 100000    | time-cost: 0.06987(s)
threads: 16  | task-type: empty task | task-numb: 1000000   | time-cost: 0.54866(s)
threads: 16  | task-type: empty task | task-numb: 10000000  | time-cost: 3.36323(s)
threads: 16  | task-type: empty task | task-numb: 100000000 | time-cost: 37.50141(s)
```

**<<测试2>>**

测试原理：调用Hipe-Steady和Hipe-Balance的**批量提交接口**提交大量的**空任务**，同时**开启**任务缓冲区限制机制，即采用有界队列。通过结果对比展示增强主线程与工作线程间**竞争**对两个线程池性能的影响。当加锁策略为每次提交一次任务就加一次锁，且由于任务为空任务，工作线程的工作时间**非常短暂**时，我们可以看到Hipe-Steady用队列交换减少竞争的**优化无法体现**。其根本原因还是任务的执行时间过短，工作线程长时间处于**饥饿状态**，主线程几乎每次添加任务都会与工作线程竞争。不同的是Hipe-Steady竞争到队列后将队列转移后执行，而Hipe-Balance竞争到队列后将任务转移后执行，而转移队列和转移任务的时间复杂度是相同的（O1）。

```
=============================================================
*   Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)   *
=============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00477(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00049(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00467(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.04435(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.45821(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 5.01119(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 52.19455(s)

==============================================================
*   Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)   *
==============================================================
threads: 16 | task-type: empty task | task-numb: 100       | time-cost: 0.00007(s)
threads: 16 | task-type: empty task | task-numb: 1000      | time-cost: 0.00587(s)
threads: 16 | task-type: empty task | task-numb: 10000     | time-cost: 0.00560(s)
threads: 16 | task-type: empty task | task-numb: 100000    | time-cost: 0.04498(s)
threads: 16 | task-type: empty task | task-numb: 1000000   | time-cost: 0.46553(s)
threads: 16 | task-type: empty task | task-numb: 10000000  | time-cost: 5.12321(s)
threads: 16 | task-type: empty task | task-numb: 100000000 | time-cost: 51.70450(s)
```





## 文件树

```
.
├── README.md
├── balanced_pond.h                      均衡线程池
├── benchmark                            性能测试文件夹 
│   ├── BS_thread_pool.hpp               BS源码
│   ├── compare_batch_submit.cpp         对比Hipe-Steady和Hipe-Balance的批量提交接口
│   ├── compare_other_task.cpp           对比Hipe-Steady和Hipe-Balance执行其它任务的性能（内存密集型任务）
│   ├── compare_submit.cpp               对比Hipe-Steady和Hipe-Balance执行空任务的性能
│   ├── makefile
│   ├── test_empty_task.cpp              测试几种线程池执行空任务的性能
│   └── test_speedup.cpp                 加速比测试
├── demo 
│   ├── demo1.cpp                        将动态线程池用作缓冲池
│   └── demo2.cpp                        动态调整动态线程池
├── dynamic_pond.h                       动态线程池
├── header.h                             定义类线程类基类和Hipe-Steady+Hipe-Balance的基类（定义了提交任务、任务溢出、负载均衡）
├── hipe.h                               头文件
├── interfaces                           测试接口
│   ├── makefile
│   ├── test_balanced_pond_interface.cpp 
│   ├── test_dynamic_pond_interface.cpp
│   └── test_steady_pond_interface.cpp
├── stability                            稳定性测试
│   ├── makefile
│   ├── test.sh                          协助测试的脚本
│   ├── test_dynamic.cpp                 
|	├── test_balance.cpp
│   └── test_steady.cpp
├── steady_pond.h                        稳定线程池
└── util.h                               工具包（任务包装器，计时器，同步输出流......）
```



## 鸣谢

一直支持我的女朋友小江和我的父母、姐姐。

《C++并发编程》

《Java并发编程实践》

BS的贡献者

小林技术交流群中的各位大佬

B站 Chunel 

## 联系我

QQ邮箱：1848395727@qq.com

