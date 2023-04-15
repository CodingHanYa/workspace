#include <workspace/workspace.h>
#include <chrono>
/**
 * Time wait for the runnable object
 * Use std::milli or std::micro or std::nano to fill template parameter
 */
template <typename Precision, typename F, typename... Args>
double timewait(F&& foo, Args&&... argv) {
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, Precision>(time_end - time_start).count();
}

/**
 * Time wait for the runnable object
 * And the precision is std::chrono::second
 */
template <typename F, typename... Args>
double timewait(F&& foo, Args&&... argv) {
    auto time_start = std::chrono::steady_clock::now();
    foo(std::forward<Args>(argv)...);
    auto time_end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(time_end - time_start).count();
}

int main(int argn, char** argvs) {
    int task_nums;
    if (argn == 2) {
        task_nums = atoi(argvs[1]);
    } else {
        perror("Invalid param");
        return -1;
    }  
    wsp::workbranch wb("bench", 4);
    auto time_cost = timewait([&]{
        int count = 0;
        for (int i = 0; i < task_nums; ++i) {
            wb.submit([]{});
        }
        wb.wait_tasks();
    });
    std::cout<<"task: "<<task_nums<<" | time-cost:"<<time_cost<<" (s)"<<std::endl;
}