#include "../hipe.h"
using namespace hipe;

// A synchronous IO stm
util::SyncStream stm;

void foo1() {
    stm.print("call foo1");
}

void foo2(std::string name) {
    stm.print(name, " call foo2");
}

struct Functor 
{
    void operator()() {
        stm.print("functor executed");
    }
};

void test_submit(SteadyThreadPond& pond) 
{
    stm.print("\n", util::boundary('=', 15), util::strong("submit"), util::boundary('=', 16));

    // no return
    pond.submit(&foo1);  // function pointer
    pond.submit([]{stm.print("HanYa say hello world");}); // lambda
    pond.submit(std::bind(foo2, "HanYa"));  // std::function<void()>
    pond.submit(Functor());

    // If you need return 
    auto ret = pond.submitForReturn([]{ return 2023; });
    stm.print("get return ", ret.get());

    // or you can do it like this
    HipeFuture<double> fut;
    HipePromise<double> pro;
    util::futureBindPromise(fut, pro);

    pond.submit([&pro]{ pro.set_value(12.25); });
    stm.print("get return ",fut.get());


    // if you need many returns
    int n = 3;
    HipePromiseVector<int> promises(n);
    HipeFutureVector<int>  futures(n);
    util::futureBindPromise(futures, promises);

    auto func = [](HipePromise<int>& pro){ pro.set_value(6); };

    for (int i = 0; i < n; ++i) {
        pond.submit(std::bind(func, std::ref(promises[i])));
    }
    for (int i = 0; i < n; ++i) {
        stm.print("get return ", futures[i].get());
    }

    
}

void test_submit_In_batch(SteadyThreadPond& pond) 
{
    stm.print("\n", util::boundary('=', 11), util::strong("submit in batch"), util::boundary('=', 11));

    // std::queue<HipeTask>;   
    // use util::block  hipe::HipeTask = hipe::util::Task;
    // std::function<void()>
    int n = 2;
    util::Block<HipeTask> blok(n);

    for (int i = 0; i < n; ++i) { 
        blok.add([i]{stm.print("block task ", i);});
    }
    pond.submitInBatch(blok, blok.element_numb());


    // use std::vector , interface reset() from HipeTask
    // std::vector<std::function<void()>> 
    // std::vector<void(*)()>
    // []
    std::vector<HipeTask> vec(n);
    for (int i = 0; i < n; ++i) {
        vec[i].reset([i]{stm.print("vector task ", i);});
    }
    pond.submitInBatch(vec, vec.size());

    // use another kind of submit by batch
    pond.submit([]{ stm.print("same task submitted two times "); }, 2);

    pond.waitForTasks();
}

void test_task_overflow() 
{
    stm.print("\n", util::boundary('=', 11), util::strong("task overflow"), util::boundary('=', 13));

    // task capacity is 100 
    SteadyThreadPond pond(10, 100); 

    pond.setRefuseCallBack([&]
    {
        stm.print("Task overflow !");
        auto blok = pond.pullOverFlowTasks();
        stm.print("Losed ", blok.element_numb(), " tasks"); // 1
    });

    util::Block<HipeTask> my_block(101);

    for (int i = 0; i < 101; ++i) {
        my_block.add([]{util::sleep_for_milli(10);});
    }

    pond.submitInBatch(my_block, 101);

}



int main() 
{
    stm.print(util::title("Test SteadyThreadPond", 10));

    // unlimited task capacity
    SteadyThreadPond pond(8);

    // unlimited task capacity pond can't set refuse callback
    // pond.setRefuseCallBack([]{stm.print("task overflow!!!");});

    test_submit(pond);
    util::sleep_for_seconds(1);

    test_submit_In_batch(pond);
    util::sleep_for_seconds(1);

    test_task_overflow(); 
    util::sleep_for_seconds(1);
   

    stm.print("\n", util::title("End of the test", 5));

}
