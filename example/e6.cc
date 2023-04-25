#include <workspace/workspace.h>

int main() {
    wsp::workspace spc;

    auto bid1 = spc.attach(new wsp::workbranch("BR1"));
    auto bid2 = spc.attach(new wsp::workbranch("BR2"));
    auto sid1 = spc.attach(new wsp::supervisor(2, 4));
    auto sid2 = spc.attach(new wsp::supervisor(2, 4));

    spc[sid1].supervise(spc[bid1]);  // start supervising
    spc[sid2].supervise(spc[bid2]);  // start supervising

    // Manual assignment
    spc.for_each([](wsp::workbranch& each){
        each.submit([]{std::cout<<std::this_thread::get_id()<<" executed task"<<std::endl;});
        each.wait_tasks();
    });

    // Automatic assignment
    spc.submit([]{std::cout<<std::this_thread::get_id()<<" executed task"<<std::endl;});
    spc.submit([]{std::cout<<std::this_thread::get_id()<<" executed task"<<std::endl;});

    spc.for_each([](wsp::workbranch& each){each.wait_tasks(1000);}); // timeout: 1000ms
}