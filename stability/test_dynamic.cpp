#include "hipe/hipe.h"
using namespace hipe;

int main() {
    hipe::DynamicThreadPond pond(4);

    std::atomic_int var(0);
    int each_task_nums = 100;

    pond.addThreads(4);     // 4 + 4 = 8
    pond.waitForThreads();  // wait for thread adjust

    assert(8 == pond.getRunningThreadNumb());

    pond.delThreads(5);  // 8-5 = 3
    pond.waitForThreads();
    pond.joinDeadThreads();

    assert(3 == pond.getRunningThreadNumb());
    assert(3 == pond.getExpectThreadNumb());

    // adjust thread numb
    pond.adjustThreads(1);  // thread number = 1

    // add tasks
    for (int i = 0; i < each_task_nums; ++i) {
        pond.submit([&] { var++; });
    }
    for (int i = 0; i < each_task_nums; ++i) {
        pond.submitForReturn([&] { var++; });
    }

    int block_size = 100;
    std::vector<std::function<void()>> block;
    block.reserve(block_size);

    for (int i = 0; i < each_task_nums / block_size; ++i) {
        for (int j = 0; j < block_size; ++j) {
            block.emplace_back([&] { var++; });
        }
        pond.submitInBatch(block, block_size);
        block.clear();
    }
    pond.waitForTasks();

    if (var.load() == each_task_nums * 3) {
        return 0;
    } else {
        return -1;
    }
}
