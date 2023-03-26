#include <hipe/hipe.h>

using namespace hipe;

util::SyncStream stream;
int thread_numb = 16;

void foo1() {
    stream.print("call foo1");
}

void test_submit_tasks(DynamicThreadPond& pond) {
    stream.print("\n", util::boundary('=', 15), util::strong("submit"), util::boundary('=', 16));

    // no return
    pond.submit([] { stream.print("hello world"); });
    pond.submit(foo1);

    // get return
    auto ret = pond.submitForReturn([] { return 2023; });
    stream.print("return = ", ret.get());

    // if you need many returns
    int n = 5;
    HipeFutures<int> futures;

    for (int i = 0; i < n; ++i) {
        futures.push_back(pond.submitForReturn([i] { return i; }));
    }

    // wait for all futures
    futures.wait();
    auto results = std::move(futures.get());

    for (auto& res : results) {
        stream.print("res = ", res);
    }

    pond.waitForTasks();
}

void test_submit_in_batch(DynamicThreadPond& pond) {
    stream.print("\n", util::boundary('=', 11), util::strong("submit by batch"), util::boundary('=', 11));

    // use std::vector
    int n = 2;
    std::vector<HipeTask> vec(n);
    for (int i = 0; i < n; ++i) {
        vec[i].reset([i] { stream.print("vector task ", i); });
    }
    pond.submitInBatch(vec, vec.size());

    // you can even do it like this
    util::repeat([&] { pond.submit([] { stream.print("submit task"); }); }, 2);

    // wait for tasks done
    pond.waitForTasks();
}

void test_motify_thread_numb(DynamicThreadPond& pond) {
    stream.print("\n", util::boundary('=', 11), util::strong("modify threads"), util::boundary('=', 11));

    pond.waitForThreads();
    stream.print("thread-numb = ", pond.getRunningThreadNumb());

    stream.print("Now we push some time consuming tasks(the count equal thread number) and delete all the threads");
    for (int i = 0; i < pond.getRunningThreadNumb(); ++i) {
        pond.submit([] { util::sleep_for_milli(300); });
    }
    // wait for all tasks got
    util::sleep_for_milli(100);
    pond.delThreads(pond.getRunningThreadNumb());

    // the threads are still working
    stream.print("Get-Running-thread-numb = ", pond.getRunningThreadNumb());
    stream.print("Get-Expect-thread-numb = ", pond.getExpectThreadNumb(), "\n");

    // wait for threads deleted
    stream.print("Wait for threads deleted ...");
    pond.waitForThreads();

    // join dead threads to recycle thread resource
    pond.joinDeadThreads();

    stream.print("Get-Running-thread-numb-again = ", pond.getRunningThreadNumb());
    stream.print("Get-Expect-thread-numb-again = ", pond.getExpectThreadNumb());

    // tasks block in task queue
    pond.submit([] { stream.print("task 1 done"); });
    pond.submit([] { stream.print("task 2 done"); });
    pond.submit([] { stream.print("task 3 done"); });

    stream.print("\nNow sleep for two seconds and then add one thread ...");  // 2s
    util::sleep_for_seconds(2);

    // tasks above start run
    pond.addThreads(1);
    pond.waitForTasks();
    pond.delThreads(1);
    pond.waitForThreads();

    stream.print("We have deleted the only one thread and now there are no threads");
    stream.print("Now we adjust the thread number to target number");

    int target_thread_number = 3;
    pond.adjustThreads(target_thread_number);
    pond.waitForThreads();
    stream.print("thread-numb now: ", pond.getRunningThreadNumb());  // 3
}

int main() {
    stream.print(util::title("Test DynamicThreadPond", 10));

    // unlimited
    DynamicThreadPond pond(thread_numb);

    util::print("\nthread-num = ", pond.getRunningThreadNumb());
    util::print("tasks-remain = ", pond.getTasksRemain());

    test_submit_tasks(pond);
    test_submit_in_batch(pond);
    test_motify_thread_numb(pond);

    stream.print("\n", util::title("End of the test", 5));
}