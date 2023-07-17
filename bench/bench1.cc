#include <workspace/workspace.h>
#include <chrono>
#include <fstream>
#include <nanobench.h>
// https://nanobench.ankerl.com/reference.html

void bench(ankerl::nanobench::Bench* bench, const char* name, size_t thread_nums, size_t task_nums) {

    wsp::workbranch wb(thread_nums);

    bench->run(name, [&]() {
        auto task = [] { };
        for (int i = 0; i < task_nums / 10; ++i) {
            wb.submit<wsp::task::seq>(task, task, task, task, task, task, task, task, task, task);
        }
        wb.wait_tasks();
    });
}

int main(int argn, char** argvs) {
    std::ofstream file("../bench1.md");

    ankerl::nanobench::Bench b;
    b.title("每次打包10个空任务,提交给workbranch 执行");
    b.relative(true);
    b.performanceCounters(true);
    b.output(&file);
    b.timeUnit(std::chrono::milliseconds{1}, "ms");
    b.minEpochIterations(500);

    bench(&b, "线程总数: 1, 任务总数: 10000", 1, 10000);
    bench(&b, "线程总数: 2, 任务总数: 10000", 2, 10000);
    bench(&b, "线程总数: 3, 任务总数: 10000", 3, 10000);
    bench(&b, "线程总数: 4, 任务总数: 10000", 4, 10000);
    bench(&b, "线程总数: 5, 任务总数: 10000", 5, 10000);
    bench(&b, "线程总数: 6, 任务总数: 10000", 6, 10000);
    bench(&b, "线程总数: 7, 任务总数: 10000", 7, 10000);
    bench(&b, "线程总数: 8, 任务总数: 10000", 8, 10000);
}
