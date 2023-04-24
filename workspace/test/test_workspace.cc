#include <workspace/workspace.h>
#include <iostream>
#define TID() std::this_thread::get_id()

int main() {
    using namespace wsp;
    wsp::workspace space;
    auto b1 = space.attach(new wsp::workbranch("Boy", 2));
    auto b2 = space.attach(new wsp::workbranch("Girl", 2));
    auto sp = space.attach(new wsp::supervisor(2, 4, 1000));

    space[sp].supervise(space[b1]);
    space[sp].supervise(space[b2]);

    // nor task A and B
    space.submit([]{std::cout<<TID()<<" exec task A"<<std::endl;});   
    space.submit([]{std::cout<<TID()<<" exec task B"<<std::endl;});

    // wait for tasks done
    space.for_each([](wsp::workbranch& each){ each.wait_tasks(); });
    
    // Detach one workbranch and there remain one.
    auto br = space.detach(b1); 
    std::cout<<"workspace still maintain: ["<<space[b2].get_name()<<"]"<<std::endl;
    std::cout<<"workspace no longer maintain: ["<<br->get_name()<<"]"<<std::endl;

    auto& ref = space.get_ref(b2);
    ref.submit<task::nor>([]{std::cout<<TID()<<" exec task C"<<std::endl;});

    // wait for tasks done
    space.for_each([](wsp::workbranch& each){ each.wait_tasks(); });

}