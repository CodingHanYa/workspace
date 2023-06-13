#include <workspace/workspace.h>
#include <iostream>
#define TID() std::this_thread::get_id()

int main() {
    using namespace wsp;
    wsp::workspace space;
    auto b1 = space.attach(new wsp::workbranch(2));
    auto b2 = space.attach(new wsp::workbranch(2));
    auto sp = space.attach(new wsp::supervisor(2, 4, 1000));

    if (b1 != b2) 
        std::cout<<"b1["<<b1<<"] != b2["<<b2<<"]"<<std::endl;
    if (b1 < b2) 
        std::cout<<"b1["<<b1<<"] <  b2["<<b2<<"]"<<std::endl;


    space[sp].supervise(space[b1]);
    space[sp].supervise(space[b2]);

    // nor task A and B
    space.submit([]{std::cout<<TID()<<" exec task A"<<std::endl;});   
    space.submit([]{std::cout<<TID()<<" exec task B"<<std::endl;});

    // wait for tasks done
    space.for_each([](wsp::workbranch& each){ each.wait_tasks(); });
    
    // Detach one workbranch and there remain one.
    auto br = space.detach(b1); 
    std::cout<<"workspace still maintain: ["<<b2<<"]"<<std::endl;
    std::cout<<"workspace no longer maintain: ["<<b1<<"]"<<std::endl;

    auto& ref = space.get_ref(b2);
    ref.submit<task::nor>([]{std::cout<<TID()<<" exec task C"<<std::endl;});

    // wait for tasks done
    space.for_each([](wsp::workbranch& each){ each.wait_tasks(); });

}