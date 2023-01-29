#include "../hipe.h"
using namespace hipe;

// =========================================================================================================
//      compare the performance of the interface "submitInBatch" between Hipe-Steady and Hipe-Balance
// =========================================================================================================

int  thread_numb = 0;
uint batch_size = 10;
uint min_task_numb = 100;
uint max_task_numb = 100000000;

void test_Hipe_steady_batch_submit() 
{
    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)"));

    hipe::SteadyThreadPond pond(thread_numb);
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

    for (uint nums = min_task_numb; nums <= max_task_numb; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-9d | time-cost: %.5f(s)\n",
        thread_numb, nums, time_cost);
    }
}


void test_Hipe_Balance_batch_submit()
{

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)"));

    hipe::BalancedThreadPond pond(thread_numb);
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

    for (uint nums = min_task_numb; nums <= max_task_numb; nums *= 10) 
    {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-9d | time-cost: %.5f(s)\n",
        thread_numb, nums, time_cost);
    }
}

//Notice that don't do two tests at once
int main() 
{
    test_Hipe_Balance_batch_submit();
    //test_Hipe_steady_batch_submit();
}

