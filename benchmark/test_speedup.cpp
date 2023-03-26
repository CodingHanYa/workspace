#include <cmath>

#include <hipe/hipe.h>
#include "./BS_thread_pool.hpp"

double single_result = 0.0;
int repeat_times = 5;

int vec_size = 4096;
int vec_nums = 2048;
std::vector<std::vector<double>> results(vec_nums, std::vector<double>(vec_size));

// computation intensive task
void computation_intensive_task() {
    for (int i = 0; i < vec_nums; ++i) {
        for (int j = 0; j < vec_size; ++j) {
            results[i][j] = std::log(std::sqrt(std::exp(std::sin(i) + std::cos(j))));
        }
    }
}

void test_single_thread() {
    hipe::util::print("\n", hipe::util::title("Test Single-thread Performance ", 12), "\n");

    int thread_numb = std::thread::hardware_concurrency();
    int task_numb = thread_numb / 4;

    auto foo1 = [](int times) {
        for (int i = 0; i < times; ++i) {
            computation_intensive_task();
        }
    };

    double total = 0.0;
    for (int j = 0; j < repeat_times; ++j) {
        total += hipe::util::timewait<std::milli>(foo1, task_numb);
    }
    double time_cost = total / repeat_times;
    double single_per_task = time_cost / task_numb;

    printf("threads: %-2d | task-type: %s | task-numb: %-2d | time-cost-per-task: %.5f(ms)\n", 1, "compute mode",
           task_numb, single_per_task);

    // time-cost per task
    single_result = single_per_task;
}

/**
 * BS_thread_pool is an open source project from github , which has gain 1k+ stars.
 * It use C++17 to construct a lightweight, fast thread pool.
 */
void test_BS() {
    int thread_numb = std::thread::hardware_concurrency();
    int task_numb = thread_numb / 4;

    hipe::util::print("\n", hipe::util::title("Test C++(17) Thread-Pool BS", 14), "\n");

    BS::thread_pool pool(thread_numb);

    auto fooN = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pool.push_task(computation_intensive_task);
        }
        pool.wait_for_tasks();
    };

    for (int i = 0; i < 6; ++i, task_numb += 12) {
        double total = 0.0;
        for (int j = 0; j < repeat_times; ++j) {
            total += hipe::util::timewait<std::milli>(fooN, task_numb);
        }
        double time_cost = total / repeat_times;
        double multi_per_task = time_cost / task_numb;

        printf("threads: %-2d | task-type: %s | task-numb: %-2d | time-cost-per-task: %.5f(ms)\n", thread_numb,
               "compute mode", task_numb, multi_per_task);
    }
}

/**
 * use Steady thread pond provided by Hipe
 */
void test_Hipe_steady() {
    int thread_numb = std::thread::hardware_concurrency();
    int task_numb = thread_numb / 4;

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread-Pool Hipe-Steady", 14), "\n");

    // use dynamic thread pond
    hipe::SteadyThreadPond pond(thread_numb);

    auto fooN = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit(computation_intensive_task);
        }
        pond.waitForTasks();
    };

    for (int i = 0; i < 6; ++i, task_numb += 12) {
        double total = 0.0;
        for (int j = 0; j < repeat_times; ++j) {
            total += hipe::util::timewait<std::milli>(fooN, task_numb);
        }
        double time_cost = total / repeat_times;
        double multi_per_task = time_cost / task_numb;

        printf("threads: %-2d | task-type: %s | task-numb: %-2d | time-cost-per-task: %.5f(ms)\n", thread_numb,
               "compute mode", task_numb, multi_per_task);
    }
}

/**
 * use dynamic thread pond provided by Hipe
 */
void test_Hipe_dynamic() {
    int thread_numb = std::thread::hardware_concurrency();
    int task_numb = thread_numb / 4;

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread-Pool Hipe-Dynamic", 14), "\n");

    // use dynamic thread pond
    hipe::DynamicThreadPond pond(thread_numb);

    auto fooN = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit(computation_intensive_task);
        }
        pond.waitForTasks();
    };

    for (int i = 0; i < 6; ++i, task_numb += 12) {
        double total = 0.0;
        for (int j = 0; j < repeat_times; ++j) {
            total += hipe::util::timewait<std::milli>(fooN, task_numb);
        }
        double time_cost = total / repeat_times;
        double multi_per_task = time_cost / task_numb;

        printf("threads: %-2d | task-type: %s | task-numb: %-2d | time-cost-per-task: %.5f(ms)\n", thread_numb,
               "compute mode", task_numb, multi_per_task);
    }
}

/**
 * use balanced thread pond provided by Hipe
 */
void test_Hipe_balance() {
    int thread_numb = std::thread::hardware_concurrency();
    int task_numb = thread_numb / 4;

    hipe::util::print("\n", hipe::util::title("Test C++(11) Thread-Pool Hipe-Balance", 14), "\n");

    // use balanced thread pond
    hipe::BalancedThreadPond pond(thread_numb);

    auto fooN = [&](int task_numb) {
        for (int i = 0; i < task_numb; ++i) {
            pond.submit(computation_intensive_task);
        }
        pond.waitForTasks();
    };

    for (int i = 0; i < 6; ++i, task_numb += 12) {
        double total = 0.0;
        for (int j = 0; j < repeat_times; ++j) {
            total += hipe::util::timewait<std::milli>(fooN, task_numb);
        }
        double time_cost = total / repeat_times;
        double multi_per_task = time_cost / task_numb;

        printf("threads: %-2d | task-type: %s | task-numb: %-2d | time-cost-per-task: %.5f(ms)\n", thread_numb,
               "compute mode", task_numb, multi_per_task);
    }
}

// Notice that don't do two tests at once
int main() {
    // test_single_thread();
    // test_BS();
    // test_Hipe_dynamic();
    // test_Hipe_steady();
    // test_Hipe_balance();
}
