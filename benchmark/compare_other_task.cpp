#include <hipe/hipe.h>

using namespace hipe;

// =============================================================================
//                 We submit some memory intensive tasks
// =============================================================================

int size = 100;
int thread_numb = 4;
int task_numb = 1000000;
int test_times = 20;

void task(int sz) {
    std::vector<int> vec(sz);
}

void test_Hipe_steady() {
    SteadyThreadPond pond(thread_numb);

    double total = 0.0;
    for (int i = 0; i < test_times; ++i) {
        double res = util::timewait([&] {
            for (int j = 0; j < task_numb; ++j) {
                pond.submit(std::bind(task, size));
            }
            pond.waitForTasks();
        });
        total += res;
    }
    printf("thread-numb: %-2d | task-numb: %-8d | test-times: %-2d | mean-time-cost: %.5f(s)\n", thread_numb, task_numb,
           test_times, total / test_times);
}

void test_Hipe_balance() {
    BalancedThreadPond pond(thread_numb);

    double total = 0.0;
    for (int i = 0; i < test_times; ++i) {
        double res = util::timewait([&] {
            for (int j = 0; j < task_numb; ++j) {
                pond.submit(std::bind(task, size));
            }
            pond.waitForTasks();
        });
        total += res;
    }
    printf("thread-numb: %-2d | task-numb: %-8d | test-times: %-2d | mean-time-cost: %.5f(s)\n", thread_numb, task_numb,
           test_times, total / test_times);
}

// Notice that don't do two tests at once
int main() {
    // util::print(util::title("Hipe-Steady Run Memory Intensive Task"));
    // test_Hipe_steady();

    util::print(util::title("Hipe-Balance Run Memory Intensive Task"));
    test_Hipe_balance();
}