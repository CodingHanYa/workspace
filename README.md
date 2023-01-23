# 一个基于C++11的高性能的、跨平台的、简单易用的线程池框架（thread pool framework）

**Hipe**是基于 C++11 编写的跨平台的、高性能的、简单易用且功能强大的线程池框架（thread pool framework），每秒能够空跑**上百万**的任务。其内置了两个职责分明的线程池（SteadyThreadPond和DynamicThreadPond），并提供了诸如任务包装器、计时器、同步IO流、自旋锁等实用的工具。
使用者可以根据业务类型结合使用Hipe-Dynamic和Hipe-Steady两种线程池来提供高并发服务。

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



## Hipe-SteadyThreadPond
（以下简称Hipe-Steady）Hipe-Steady是Hipe提供的稳定的、具有固定线程数的线程池。支持批量提交任务和批量执行任务、支持有界任务队列和无界任务队列、支持池中线程的**任务窃取**。任务溢出时支持**注册回调**并执行或者**抛出异常**。

Hipe-Steady内部为每个线程都分配了公开任务队列、缓冲任务队列和控制线程的同步变量（thread-local机制），尽量降低**乒乓缓存**和**线程同步**对线程池性能的影响。工作线程通过队列替换**批量下载**公开队列的任务到缓冲队列中执行。生产线程则通过公开任务队列为工作线程**分配任务**（采用了一种优于轮询的**负载均衡**机制）。通过公开队列和缓冲队列（或说私有队列）替换的机制进行**读写分离**，再通过加**轻锁**（C++11原子量实现的自旋锁）的方式极大地提高了线程池的性能。

由于其底层的实现机制，Hipe-Steady适用于**稳定的**（避免超时任务阻塞线程）、**任务量大**（任务传递的优势得以体现）的任务流。也可以说Hipe-Steady适合作为核心线程池（能够处理基准任务并长时间运行），而当可以**定制容量**的Hipe-Steady面临任务数量超过设定值时—— 即**任务溢出**时，我们可以通过定制的**回调函数**拉取出溢出的任务，并把这些任务推到我们的动态线程池DynamicThreadPond中。在这个情景中，DynamicThreadPond或许可以被叫做CacheThreadPond。关于二者之间如何协调运作，大家可以阅读`Hipe/demo/demo1`.在这个demo中我们展示了如何把DynamicThreadPond用作Hipe-Steady的缓冲池。


## Hipe-DynamicThreadPond

（以下简称Hipe-Dynamic）Hipe-Dynamic是Hipe提供的动态的、能够扩缩容的线程池。支持批量提交任务、支持线程吞吐任务速率监测、支持无界队列。当没有任务时所有线程会被自动挂起（阻塞等待条件变量的通知），较为节能。

Hipe-Dynamic采用的是多线程竞争单条任务队列的模型。该任务队列是无界的，能够容蓄大量的任务（直至系统资源耗尽）。由于Hipe-Dynamic管理的线程没有私有的任务队列，因此能够被灵活地调度。同时，为了能动态调节线程数，Hipe-Dynamic还提供了能监测线程池执行速率的接口，其使用实例在`Hipe/demo/demo2`。

由于Hipe-Dynamic的接口较为简单，如果需要了解更多接口的调用，可以阅读接口测试文件`Hipe/interface_test/`或者`Hipe/demo/demo2`。



## Performance BenchMark

[bshoshany](https://github.com/bshoshany)/**[thread-pool](https://github.com/bshoshany/thread-pool)** （以下简称BS）是在GitHub上开源的已收获了**1k+stars** 的C++线程池，采用C++17编写，具有轻量，高效的特点。我们通过**加速比测试和空任务测试**，对比BS和Hipe的性能。实际上BS的底层机制与Hipe-Dynamic相似，都是多线程竞争一条任务队列，并且在没有任务时被条件变量阻塞。

测试机器：16核_ubuntu20.04

### 加速比测试

- 测试原理： 通过执行计算密集型的长任务，与单线程进行对比，进而算出线程池的加速比。每次测试都会重复5遍并取平均值。

```C++
// 任务类型
uint vec_size = 4096;
uint vec_nums = 2048;
std::vector<std::vector<double>> results(vec_nums, std::vector<double>(vec_size));

// computation intensive task(计算密集型任务)
void computation_intensive_task() {
    for (int i = 0; i < vec_nums; ++i) {
        for (size_t j = 0; j < vec_size; ++j) {
            results[i][j] = std::log(std::sqrt(std::exp(std::sin(i) + std::cos(j))));
        }
    }
}
```



```

=======================================================
*           Test Single-thread Performance            *
=======================================================

threads: 1  | task-type: compute mode | task-numb: 4  | time-cost-per-task: 349.40249(ms)

================================================================
*             Test C++(11) Thread-Pool Hipe-Steady             *
================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 139.38149(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 37.13697(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 44.36706(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 47.09633(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 48.13259(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 45.09768(ms)
Best speed-up obtained by multithreading vs. single-threading: 9.41, using 16 tasks

=======================================================
*             Test C++(17) Thread-Pool BS             *
=======================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 93.45621(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 41.98891(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 44.13553(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 44.37572(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 44.79318(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 39.93736(ms)
Best speed-up obtained by multithreading vs. single-threading: 8.75, using 64 tasks

=================================================================
*             Test C++(11) Thread-Pool Hipe-Dynamic             *
=================================================================

threads: 16 | task-type: compute mode | task-numb: 4  | time-cost-per-task: 94.31042(ms)
threads: 16 | task-type: compute mode | task-numb: 16 | time-cost-per-task: 40.00866(ms)
threads: 16 | task-type: compute mode | task-numb: 28 | time-cost-per-task: 43.75092(ms)
threads: 16 | task-type: compute mode | task-numb: 40 | time-cost-per-task: 44.70085(ms)
threads: 16 | task-type: compute mode | task-numb: 52 | time-cost-per-task: 45.11398(ms)
threads: 16 | task-type: compute mode | task-numb: 64 | time-cost-per-task: 39.82556(ms)
Best speed-up obtained by multithreading vs. single-threading: 8.77, using 64 tasks

=============================================
*              End of the test              *
=============================================

```

- 结果分析：可以看到线程池BS的最佳加速比为**8.75倍**， Hipe-Steady线程池的最佳加速比为**9.41倍**，Hipe-Dynamic的最佳加速比为**8.77倍**。三者的性能接近，说明在任务传递过程开销较小的情况下（由于任务数较少），**乒乓缓存、线程切换和线程同步**等因素对三种种线程池的加速比的影响是相近的。同时我们注意到Hipe-Steady在任务数为16时（与线程数相同时）有最好的表现，这其实与Hipe-Steady的底层负载均衡的机制有关。有兴趣的朋友可以尝试解释一下。

### 空任务测试

- 测试原理： 通过提交大量的空任务到线程池中，对比两种线程池处理空任务的能力，其主要影响因素为**任务传递**、**线程同步**等的开销。

```

=============================================
*   Test C++(11) Thread Pool Hipe-Dynamic   *
=============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00142(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01066(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.09554(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.96166(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.72766(s)

===================================
*   Test C++(17) Thread Pool BS   *
===================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00160(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.01204(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.10107(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.97874(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 9.83712(s)

============================================
*   Test C++(11) Thread Pool Hipe-Steady   *
============================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00067(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00063(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00673(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.06083(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.61471(s)

=============================================================
*   Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)   *
=============================================================
threads: 16 | task-type: empty task | task-numb: 100      | time-cost: 0.00003(s)
threads: 16 | task-type: empty task | task-numb: 1000     | time-cost: 0.00027(s)
threads: 16 | task-type: empty task | task-numb: 10000    | time-cost: 0.00280(s)
threads: 16 | task-type: empty task | task-numb: 100000   | time-cost: 0.02907(s)
threads: 16 | task-type: empty task | task-numb: 1000000  | time-cost: 0.26858(s)
threads: 16 | task-type: empty task | task-numb: 10000000 | time-cost: 2.79028(s)

=============================================
*              End of the test              *
=============================================

```

- 结果分析： 可以看到在处理空任务这一方面Hipe-Steady具有**巨大的优势**，在处理**1000000**个空任务时性能是BS和Hipe-Dynamic的**10倍以上**。如果采用批量提交接口能达到约**30倍以上**的性能（注意！我们测试批量提交任务的时候最后用的是一千万个任务哦）。而且随着任务数增多Steady线程池也并未呈现出指数级的增长趋势，而是呈常数级的增长趋势。即随着任务增多而线性增长。



## 文件树

```
.
├── README.md                            本文档
├── benchmark                            性能测试
│   ├── BS_thread_pool.hpp               BS线程池
│   ├── makefile
│   ├── test_empty_task.cpp              跑空任务
│   └── test_speedup.cpp                 测加速比
├── demo
│   ├── demo1.cpp                        如何将Hipe-Dynamic作为缓冲池
│   └── demo2.cpp                        如何根据流量动态调节Hipe-Dynamic的线程数
├── dynamic_pond.h                       Hipe-Dynamic
├── header.h                             一些别名和引入的头文件	
├── hipe.h                               方便导入的文件，已将Hipe的头文件包含了 
├── interface_test                       接口测试
│   ├── makefile
│   ├── test_dynamic_pond_interface.cpp  Hipe-Dynamic的接口测试
│   └── test_steady_pond_interface.cpp   Hipe-Steady的接口测试
├── steady_pond.h                        Hipe-Steady
└── util.h                               工具包：计时器、任务包装器、同步IO流...
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

