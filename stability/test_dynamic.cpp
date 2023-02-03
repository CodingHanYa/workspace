#include "../hipe.h"
using namespace hipe;

int main() 
{
    hipe::DynamicThreadPond pond(8);

    std::atomic_int var(0);
    int each_task_nums = 100;

    for (int i = 1; i < 20; ++i) {
        pond.addThreads(i);
        pond.delThreads(i-1);
    }
    pond.adjustThreads(8);

    for (int i = 0; i < each_task_nums; ++i) {
        pond.submit([&]{ var++; });
    }
    for (int i = 0; i < each_task_nums; ++i) {
        pond.submitForReturn([&]{ var++; });
    }

    int block_size = 100;
    util::Block<std::function<void()>> block(block_size);

    for (int i = 0; i < each_task_nums/block_size; ++i) 
    {
        for (int j = 0; j < block_size; ++j) {
            block.add([&]{ var++; });
        }
        pond.submitInBatch(block, block_size);
        block.clean();
    } 
    pond.waitForTasks();

    if (var.load() == each_task_nums*3) {
        return 0;
    } else {
        return -1;
    }
}
