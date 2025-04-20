#include "timewait.h"
#include "workspace/workspace.hpp"

class Obj {
    char pad[64];
};

int main(int argn, char** argvs) {
    int task_nums, thread_nums;
    if (argn == 3) {
        thread_nums = atoi(argvs[1]);
        task_nums = atoi(argvs[2]);
    } else {
        fprintf(stderr, "Invalid parameter! usage: [threads + tasks]\n");
        return -1;
    }
    wsp::workbranch wb(thread_nums, wsp::waitstrategy::balance);
    auto time_cost = timewait([&] {
        for (int i = 0; i < task_nums; ++i) {
            wb.submit([]{});
        }
        wb.wait_tasks();
    });
    std::cout << "threads: " << std::left << std::setw(2) << thread_nums << " |  tasks: " << task_nums
              << "  |  time-cost: " << time_cost << " (s)" << std::endl;
}