#include <workspace/workspace.h>

int main() {
    wsp::workbranch br1(2);
    wsp::workbranch br2(2);

    // 2 <= thread number <= 4 
    // time interval: 1000 ms 
    wsp::supervisor sp(2, 4, 1000);

    sp.set_tick_cb([&br1, &br2]{
        auto now = std::chrono::system_clock::now();
        std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
        std::tm local_time = *std::localtime(&timestamp);
        static char buffer[40];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_time);
        std::cout<<"["<<buffer<<"] "<<"br1: [workers] "<<br1.num_workers()<<" | [blocking-tasks] "<<br1.num_tasks()<<'\n';
        std::cout<<"["<<buffer<<"] "<<"br2: [workers] "<<br2.num_workers()<<" | [blocking-tasks] "<<br2.num_tasks()<<'\n';
    });

    sp.supervise(br1);  // start supervising
    sp.supervise(br2);  // start supervising

    for (int i = 0; i < 1000; ++i) {
        br1.submit([]{std::this_thread::sleep_for(std::chrono::milliseconds(10));});
        br2.submit([]{std::this_thread::sleep_for(std::chrono::milliseconds(20));});
    }

    br1.wait_tasks();
    br2.wait_tasks();
}