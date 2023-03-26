#include <hipe/hipe.h>

// =========================================================================================================
//       compare the performance of the interface "submit" between Hipe-Steady and Hipe-Balance
// =========================================================================================================

int task_numb = 1000000;
int max_thread_numb = 50;

int main() {
    for (int t = 1; t <= max_thread_numb; ++t) {
        hipe::SteadyThreadPond pond1(t);
        hipe::BalancedThreadPond pond2(t);

        double res1 = hipe::util::timewait([&] {
            for (int i = 0; i < task_numb; ++i) {
                pond1.submit([] {});
            }
            pond1.waitForTasks();
        });

        double res2 = hipe::util::timewait([&] {
            for (int i = 0; i < task_numb; ++i) {
                pond2.submit([] {});
            }
            pond2.waitForTasks();
        });

        printf("Task-Numb: %-8d | Thread-Numb: %-3d | Steady-Time-Cost: %.5f  |  Balanced-Time-Cost: %.5f\n", task_numb,
               t, res1, res2);
    }
}