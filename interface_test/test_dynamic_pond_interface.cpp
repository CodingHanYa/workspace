#include "../hipe.h"
using namespace hipe;

util::SyncStream stm;
uint thread_numb = 16;

void foo1() {
    stm.print("call foo1");
}

// If you want result
void foo2(HipePromise<int>& result) {
    result.set_value(2023);
}

void test_submit_tasks(DynamicThreadPond& pond) 
{
    stm.print("\n", util::boundary('=', 15), util::strong("submit"), util::boundary('=', 16));

    // no return
    pond.submit([]{stm.print("hello world");});
    pond.submit(foo1);

    // get return
    HipePromise<int> pro;
    HipeFuture<int> fut;
    util::futureBindPromise(fut, pro);

    pond.submit(std::bind(foo2, std::move(pro)));
    stm.print("return = ", fut.get());

    // get returns
    int n = 3;

    HipeFutureVector<int> futures(n);
    HipePromiseVector<int> promises(n);
    util::futureBindPromise(futures, promises);

    for (int i = 0; i < n; ++i) {
        pond.submit([&promises, i]{ promises[i].set_value(i); });
    }
    for (int j = 0; j < n; ++j) {
        stm.print("return = ", futures[j].get());
    }
    pond.waitForTasks();

}

void test_submit_by_batch(DynamicThreadPond& pond) 
{
    stm.print("\n", util::boundary('=', 11), util::strong("submit by batch"), util::boundary('=', 11));

    // use util::block
    int n = 2;
    util::Block<HipeTask> blok(n);
    for (int i = 0; i < n; ++i) { 
        blok.add([i]{stm.print("block task ", i);});
    }
    pond.submitInBatch(blok, blok.element_numb());


    // use std::vector;
    std::vector<HipeTask> vec(n);
    for (int i = 0; i < n; ++i) {
        vec[i].reset([i]{stm.print("vector task ", i);});
    }
    pond.submitInBatch(vec, vec.size());

    // another kind of submit by batch
    pond.submit([]{stm.print("submit task");}, 2);

    // wait for tasks done
    pond.waitForTasks();
}

void test_motify_thread_numb(DynamicThreadPond& pond) 
{
    stm.print("\n", util::boundary('=', 11), util::strong("modify threads"), util::boundary('=', 11));

    stm.print("thread-numb = ", pond.getThreadNumb());
    stm.print("Now delete all the threads");

    pond.delThreads(pond.getThreadNumb());
    stm.print("thread-numb = ", pond.getThreadNumb(), "\n");    // 0

    pond.submit([]{stm.print("task 1 done");});
    pond.submit([]{stm.print("task 2 done");});
    pond.submit([]{stm.print("task 3 done");});

    stm.print("Now sleep for two seconds and then add one thread ...");  // 2s
    util::sleep_for_seconds(2);

    pond.addThreads(1);
    pond.waitForTasks();
    pond.delThreads(1);

    stm.print("We have deleted the only one thread and now there are no threads");
    stm.print("Now we adjust the thread number to target number");


    int target_thread_number = 3;
    pond.adjustThreads(target_thread_number);
    stm.print("thread-numb now: ", pond.getThreadNumb());  // 3

}


int main() 
{
    stm.print(util::title("Test DynamicThreadPond", 10));

    // unlimited
    DynamicThreadPond pond(thread_numb);

    util::print("\nthread-num = ", pond.getThreadNumb());
    util::print("tasks-remain = ", pond.getTasksRemain());
    
    test_submit_tasks(pond);
    test_submit_by_batch(pond);
    test_motify_thread_numb(pond);

    stm.print("\n", util::title("End of the test", 5));

}