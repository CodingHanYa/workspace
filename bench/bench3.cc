#include <workspace/workspace.h>
#include <chrono>
#include <fstream>
#include <nanobench.h>
// https://nanobench.ankerl.com/reference.html

void bench(ankerl::nanobench::Bench* bench, const char* name, size_t thread_nums, size_t task_nums) {

    wsp::workspace spc;
    for (int i = 0; i < thread_nums/2; ++i) {
        spc.attach(new wsp::workbranch(2));
    }

    bench->run(name, [&]() {
        auto task = []{/* empty task */};
        for (int i = 0; i < task_nums/10; ++i) {
            spc.submit<wsp::task::seq>(task, task, task, task, task, task, task, task, task, task);
        }
        spc.for_each([](wsp::workbranch& each){each.wait_tasks();});
    });
}

int main(int argn, char** argvs) {
    std::ofstream file("../bench3.md");

    ankerl::nanobench::Bench b;
    b.title("每次打包10个空任务,提交给workspace执行, workspace管理的每个workbranch中都拥有2条线程");
    b.relative(true);
    b.performanceCounters(true);
    b.output(&file);
    b.timeUnit(std::chrono::milliseconds{1}, "ms");
    b.minEpochIterations(500);


    bench(&b, "线程总数: 2, 任务总数: 10000", 2, 10000);
    bench(&b, "线程总数: 4, 任务总数: 10000", 4, 10000);
    bench(&b, "线程总数: 6, 任务总数: 10000", 6, 10000);
    bench(&b, "线程总数: 8, 任务总数: 10000", 8, 10000);
    bench(&b, "线程总数: 10, 任务总数: 10000", 10, 10000);
    bench(&b, "线程总数: 12, 任务总数: 10000", 12, 10000);
}
