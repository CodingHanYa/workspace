#include "../hipe.h"
#include "./BS_thread_pool.hpp"

uint min_task_numb = 100;


void test_BS() 
{
    uint tnumb = std::thread::hardware_concurrency();
    BS::thread_pool pool(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(17) Thread Pool BS"));

    auto foo = [&](uint task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pool.push_task([]{});
        }
        pool.wait_for_tasks();
    };
    for (uint nums = min_task_numb; nums <= 1000000; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n",
        tnumb, nums, time_cost);
    }
}

void test_Hipe_steady() 
{
    uint tnumb = std::thread::hardware_concurrency();
    hipe::SteadyThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Steady"));

    auto foo = [&](uint task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit([]{});
        }
        pond.waitForTasks();
    };
    for (uint nums = min_task_numb; nums <= 1000000; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n",
        tnumb, nums, time_cost);
    }
}

void test_Hipe_steady_batch_submit() 
{
    uint tnumb = std::thread::hardware_concurrency();
    hipe::SteadyThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)"));

    uint batch_size = 10;

    hipe::util::Block<hipe::HipeTask> task_block(batch_size);

    auto foo = [&](uint task_numb) 
    {
        for (int i = 0; i < task_numb;) 
        {
            for (int j = 0; j < batch_size; ++j, ++i) {
                task_block.add([]{});
            }
            pond.submitInBatch(task_block, batch_size);
            task_block.clean();
        }
        pond.waitForTasks();
    };
    for (uint nums = min_task_numb; nums <= 10000000; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n",
        tnumb, nums, time_cost);
    }
}

void test_Hipe_dynamic() 
{
    uint tnumb = std::thread::hardware_concurrency();
    hipe::DynamicThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Dynamic"));

    auto foo = [&](uint task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit([]{});
        }
        pond.waitForTasks();
    };
    for (uint nums = min_task_numb; nums <= 1000000; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n",
        tnumb, nums, time_cost);
    }
}

int main() 
{
    test_Hipe_dynamic();

    hipe::util::sleep_for_seconds(5);

    test_BS();

    hipe::util::sleep_for_seconds(5);

    test_Hipe_steady();

    hipe::util::sleep_for_seconds(2);

    test_Hipe_steady_batch_submit();

    hipe::util::print("\n", hipe::util::title("End of the test", 15));
}
