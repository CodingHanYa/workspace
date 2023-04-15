#include <workspace/workspace.h>

int main() {

    wsp::workspace space;   // An object manager 

    auto x = space.attach(new wsp::workbranch("X"));        // X: default one threads
    auto y = space.attach(new wsp::workbranch("Y"));        // Y: default one threads
    auto z = space.attach(new wsp::supervisor(1, 4, 1000)); // Z: 1s per check
    space[z].supervise(space[x]);
    space[z].supervise(space[y]);

    space[x].submit([]{std::cout<<"========  Test Submit  ======\n";});  // 1. normal task
    space[x].wait_tasks();

    space[x].submit<wsp::nor>([]{std::cout<<"Second task done\n";});     // 2. normal task (the same as 1.)
    space[x].submit<wsp::urg>([]{std::cout<<"First task done\n";});      // 3. urgent task (no guarantee of advance)
    space[y].submit([]{std::cout<<"continue...\n";});                    // to y 

    space.submit([]{std::cout<<std::this_thread::get_id()<<" finished this task\n";}); // distribution
    space.submit([]{std::cout<<std::this_thread::get_id()<<" finished this task\n";}); // distribution

    auto ret1 = space[x].submit<wsp::nor>([]{return 1;});   // return future
    auto ret2 = space[x].submit<wsp::urg>([]{return 2;});   // return future

    std::cout<<"got return: "<<ret1.get()<<std::endl;
    std::cout<<"got return: "<<ret2.get()<<std::endl;

    auto ret3 = space.submit([]{ return "task3"; });        // return future
    auto ret4 = space.submit([]{ return "task4"; });        // return future

    std::cout<<"got return: "<<ret3.get()<<std::endl;
    std::cout<<"got return: "<<ret4.get()<<std::endl;

    space.for_each([](wsp::workbranch& br){ br.wait_tasks(); });
    space.for_each([](wsp::supervisor& sp){ sp.suspend(); });
    space[y].submit([]{std::cout<<"========  End of Test  ======\n";});
    space[y].wait_tasks();

}