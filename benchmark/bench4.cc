#include <workspace/workspace.hpp>

#include "timewait.h"

int main(int argn, char** argvs) {
    int task_nums, thread_nums;
    if (argn == 3) {
        thread_nums = atoi(argvs[1]);
        task_nums = atoi(argvs[2]);
    } else {
        fprintf(stderr, "Invalid parameter! usage: [threads + tasks]\n");
        return -1;
    }
    for (auto strategy : {wsp::waitstrategy::lowlatancy, wsp::waitstrategy::balance, wsp::waitstrategy::blocking}) {
        wsp::workspace spc;
        for (int i = 0; i < thread_nums; ++i) {
            spc.attach(new wsp::workbranch(1, strategy));
        }
        auto time_cost = timewait([&] {
            auto task = [] { /* empty task */ };
            for (int i = 0; i < task_nums / 10; ++i) {
                spc.submit<wsp::task::seq>(task, task, task, task, task, task, task, task, task, task);
            }
            spc.for_each([](wsp::workbranch& each) { each.wait_tasks(); });
        });
        const char* strategy_name = "";
        switch (strategy) {
            case wsp::waitstrategy::lowlatancy:
                strategy_name = "lowlatancy";
                break;
            case wsp::waitstrategy::balance:
                strategy_name = "balance";
                break;
            case wsp::waitstrategy::blocking:
                strategy_name = "blocking";
                break;
        }
        std::cout << "Strategy: " << std::left << std::setw(15) << strategy_name << " | Threads: " << std::setw(2)
                  << thread_nums << " | Tasks: " << std::setw(8) << task_nums << " | Time-cost: " << time_cost << " (s)"
                  << std::endl;
    }
}