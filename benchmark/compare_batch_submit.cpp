#include <hipe/hipe.h>
using namespace hipe;

// =========================================================================================================
//      compare the performance of the interface "submitInBatch" between Hipe-Steady and Hipe-Balance
// =========================================================================================================

int thread_numb = 16;
int batch_size = 10;
int min_task_numb = 100;
int max_task_numb = 100000000;

void test_Hipe_steady_batch_submit() {
    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Steady-Batch-Submit(10)"));

    // hipe::SteadyThreadPond pond(thread_numb, thread_numb * 1000);
    hipe::SteadyThreadPond pond(thread_numb);
    std::vector<hipe::HipeTask> tasks;
    tasks.reserve(batch_size);

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb;) {
            for (int j = 0; j < batch_size; ++j, ++i) {
                tasks.emplace_back([] {});
            }
            pond.submitInBatch(tasks, batch_size);
            tasks.clear();
        }
        pond.waitForTasks();
    };

    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-9d | time-cost: %.5f(s)\n", thread_numb, nums,
               time_cost);
    }
}

void test_Hipe_Balance_batch_submit() {
    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Balance-Batch-Submit(10)"));

    // hipe::BalancedThreadPond pond(thread_numb, thread_numb * 1000);
    hipe::BalancedThreadPond pond(thread_numb);
    std::vector<hipe::HipeTask> tasks;
    tasks.reserve(batch_size);

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb;) {
            for (int j = 0; j < batch_size; ++j, ++i) {
                tasks.emplace_back([] {});
            }
            pond.submitInBatch(tasks, batch_size);
            tasks.clear();
        }
        pond.waitForTasks();
    };

    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-9d | time-cost: %.5f(s)\n", thread_numb, nums,
               time_cost);
    }
}

// Notice that don't do two tests at once
int main() {
    test_Hipe_Balance_batch_submit();
    // test_Hipe_steady_batch_submit();
}
