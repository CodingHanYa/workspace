/**
 * How to use Hipe-Dynamic as a buffer pool
 */
#include "../hipe.h"

int main() {
    // thread number = core number - 1
    int thread_numb = static_cast<int>(std::thread::hardware_concurrency()) - 1;

    hipe::SteadyThreadPond core_pond(thread_numb, thread_numb * 10);
    hipe::DynamicThreadPond cach_pond(thread_numb / 2);
    std::atomic_int var(0);

    // pass the task to cach pond
    core_pond.setRefuseCallBack([&] {
        auto task_block = core_pond.pullOverFlowTasks();
        cach_pond.submitInBatch(task_block, task_block.element_numb());
        hipe::util::print("Overflow task number = ", task_block.element_numb());
    });

    // overflow one task
    for (int i = 0; i < (thread_numb * 10 + 1); ++i) {
        core_pond.submit([&] {
            hipe::util::sleep_for_micro(2);
            var++;
        });
    }
    core_pond.waitForTasks();
    cach_pond.waitForTasks();

    if (var.load() == (thread_numb * 10 + 1)) {
        hipe::util::print("All task done");
    }
}