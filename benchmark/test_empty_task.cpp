#include <hipe/hipe.h>
#include "./BS_thread_pool.hpp"

int min_task_numb = 100;
int max_task_numb = 1000000;

// dynamic thread pond
void test_Hipe_dynamic() {
    int tnumb = std::thread::hardware_concurrency();
    hipe::DynamicThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Dynamic"));

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit([] {});
        }
        pond.waitForTasks();
    };
    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n", tnumb, nums,
               time_cost);
    }
}

// BS thread pool
void test_BS() {
    int tnumb = std::thread::hardware_concurrency();
    BS::thread_pool pool(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(17) Thread Pool BS"));

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pool.push_task([] {});
        }
        pool.wait_for_tasks();
    };
    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n", tnumb, nums,
               time_cost);
    }
}

// SteadyThreadPond
void test_Hipe_steady() {
    int tnumb = std::thread::hardware_concurrency();
    hipe::SteadyThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Steady"));

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit([] {});
        }
        pond.waitForTasks();
    };
    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n", tnumb, nums,
               time_cost);
    }
}

// BalancedThreadPond
void test_Hipe_balance() {
    int tnumb = std::thread::hardware_concurrency();
    hipe::BalancedThreadPond pond(tnumb);

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread Pool Hipe-Balance"));

    auto foo = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit([] {});
        }
        pond.waitForTasks();
    };
    for (int nums = min_task_numb; nums <= max_task_numb; nums *= 10) {
        double time_cost = hipe::util::timewait(foo, nums);
        printf("threads: %-2d | task-type: empty task | task-numb: %-8d | time-cost: %.5f(s)\n", tnumb, nums,
               time_cost);
    }
}

int main() {
    test_BS();

    hipe::util::sleep_for_seconds(5);

    test_Hipe_dynamic();

    hipe::util::sleep_for_seconds(5);

    test_Hipe_steady();

    hipe::util::sleep_for_seconds(5);

    test_Hipe_balance();

    hipe::util::sleep_for_seconds(5);

    hipe::util::print("\n", hipe::util::title("End of the test", 15));
}
