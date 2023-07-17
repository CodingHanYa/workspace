#include <workspace/workspace.h>
#include <chrono>
#include <fstream>
#include <nanobench.h>
// https://nanobench.ankerl.com/reference.html

void bench(ankerl::nanobench::Bench* bench, const char* name, size_t thread_nums, size_t task_nums) {

   wsp::workspace spc;
    for (int i = 0; i < thread_nums; ++i) {
        spc.attach(new wsp::workbranch());
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
    std::ofstream file("../bench2.md");

    ankerl::nanobench::Bench b;
    b.title("每次打包10个空任务,提交给workspace执行, workspace管理的每个workbranch中都拥有1条线程");
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
