#include <hipe/hipe.h>
using namespace hipe;

// A synchronous IO stream
util::SyncStream stream;

void foo1() {
    stream.print("call foo1");
}

void foo2(std::string name) {
    stream.print(name, " call foo2");
}

struct Functor {
    void operator()() {
        stream.print("functor executed");
    }
};

void test_submit(SteadyThreadPond& pond) {
    stream.print("\n", util::boundary('=', 15), util::strong("submit"), util::boundary('=', 16));

    // no return
    pond.submit(&foo1);                                          // function pointer
    pond.submit([] { stream.print("HanYa say hello world"); });  // lambda
    pond.submit(std::bind(foo2, "HanYa"));                       // std::function<void()>
    pond.submit(Functor());                                      // functor

    // If you need return
    auto ret = pond.submitForReturn([] { return 2023; });
    stream.print("get return ", ret.get());

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
}

void test_submit_in_batch(SteadyThreadPond& pond) {
    stream.print("\n", util::boundary('=', 11), util::strong("submit in batch"), util::boundary('=', 11));

    int n = 2;

    // use std::vector
    // the vector has overloaded []
    std::vector<HipeTask> vec;

    for (int i = 0; i < n; ++i) {
        vec.emplace_back([i] { stream.print("vector task ", i); });
    }
    pond.submitInBatch(vec, vec.size());

    // you can even do it like this
    util::repeat([&] { pond.submit([] { stream.print("submit task"); }); }, 2);

    pond.waitForTasks();
}

void test_task_overflow() {
    stream.print("\n", util::boundary('=', 11), util::strong("task overflow"), util::boundary('=', 13));

    // task capacity is 100
    SteadyThreadPond pond(10, 100);

    pond.setRefuseCallBack([&] {
        stream.print("Task overflow !");
        auto blok = std::move(pond.pullOverFlowTasks());
        stream.print("Losed ", blok.size(), " tasks");  // 1
    });

    std::vector<HipeTask> my_block;

    for (int i = 0; i < 101; ++i) {
        my_block.emplace_back([] { util::sleep_for_milli(10); });
    }

    pond.submitInBatch(my_block, 101);
}

void test_other_interface(SteadyThreadPond& pond, int thread_numb) {
    stream.print("\n", util::boundary('=', 11), util::strong("other interface"), util::boundary('=', 13));

    util::print("enable rob tasks");

    // enable steal neighbor thread and reduce the impact of abnormal tasks blocking threads
    pond.enableStealTasks(thread_numb / 2);

    util::print("disable rob tasks");
    // than we just disable this function
    pond.disableStealTasks();
}

int main() {
    stream.print(util::title("Test SteadyThreadPond", 10));

    // unlimited task capacity
    SteadyThreadPond pond(8, 800);

    // unlimited task capacity pond can't set refuse callback
    // pond.setRefuseCallBack([]{stream.print("task overflow!!!");});

    test_submit(pond);
    util::sleep_for_seconds(1);

    test_submit_in_batch(pond);
    util::sleep_for_seconds(1);

    test_task_overflow();
    util::sleep_for_seconds(1);

    test_other_interface(pond, 8);
    util::sleep_for_seconds(1);

    stream.print("\n", util::title("End of the test", 5));
}
