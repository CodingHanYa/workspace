#include <workspace/workspace.h>

int main() {
    wsp::futures<int> futures;
    wsp::workspace spc;
    spc.attach(new wsp::workbranch(2));
    
    futures.add_back(spc.submit([]{return 1;}));
    futures.add_back(spc.submit([]{return 2;}));

    // wait all tasks done and get the results
    futures.wait();  
    auto res = futures.get();
    for (auto& each: res) {
        std::cout<<"got "<<each<<std::endl;
    }
}