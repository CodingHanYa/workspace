/**
 * How to adjust the dynamic thread pool.
 * This is just a reference.
 */
#include "hipe/hipe.h"

bool closed = false;
int g_tnumb = 8;
int tasks_numb = 15000;
int milli_per_submit = 500;  // 0.5s

void manager(hipe::DynamicThreadPond* pond) {
    enum class Action { add, del };
    Action last_act = Action::add;

    int unit = 2;
    int prev_load = 0;
    int max_thread_numb = 200;
    int min_thread_numb = 8;
    int total = 0;

    while (!closed) {
        auto new_load = pond->resetTasksLoaded();
        auto tnumb = pond->getExpectThreadNumb();
        total += new_load;

        printf("threads: %-3d remain: %-4d loaded: %d\n", tnumb, pond->getTasksRemain(), new_load);
        fflush(stdout);

        // if (!prev_load),  we still try add threads and have a look at the performance
        if (new_load > prev_load) {
            if (tnumb < max_thread_numb) {
                pond->addThreads(unit);
                pond->waitForThreads();
                last_act = Action::add;
            }

        } else if (new_load < prev_load) {
            if (last_act == Action::add && tnumb > min_thread_numb) {
                pond->delThreads(unit);
                pond->waitForThreads();
                last_act = Action::del;
            } else if (last_act == Action::del) {
                pond->addThreads(unit);
                pond->waitForThreads();
                last_act = Action::add;
            }
        } else {
            if (!pond->getTasksRemain() && tnumb > min_thread_numb) {
                pond->delThreads(unit);
                pond->waitForThreads();
                last_act = Action::del;
            }
        }
        prev_load = new_load;

        // I think that the interval of each sleep should be an order of magnitude more than the time a task cost.
        hipe::util::sleep_for_seconds(1);  // 1s
    }

    total += pond->resetTasksLoaded();
    hipe::util::print("total load ", total);
}

int main() {
    hipe::DynamicThreadPond pond(g_tnumb);

    // total 0.1s
    auto task1 = [] { hipe::util::sleep_for_milli(20); };
    auto task2 = [] { hipe::util::sleep_for_milli(30); };
    auto task3 = [] { hipe::util::sleep_for_milli(50); };

    auto tasks_per_second = 600;

    hipe::util::print("Submit ", tasks_per_second, " task per second");
    hipe::util::print("So we hope that the threads is able to load [", tasks_per_second, "] task per second");
    hipe::util::print(hipe::util::boundary('=', 65));

    // create a manager thread.
    std::thread mger(manager, &pond);

    // 600 tasks per second
    int count = (tasks_numb / 3) / 100;
    while (count--) {
        // 300 tasks
        for (int i = 0; i < 100; ++i) {
            pond.submit(task1);
            pond.submit(task2);
            pond.submit(task3);
        }
        hipe::util::sleep_for_milli(500);  // 0.5s
    }

    // wait for task done and than close the manager
    pond.waitForTasks();

    closed = true;
    mger.join();
}