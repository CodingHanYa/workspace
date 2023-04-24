#include <workspace/workspace.h>

int main() {
    wsp::futures<int> futures;
    wsp::workspace spc;
    spc.attach(new wsp::workbranch("br", 2));
    
    futures.add_back(spc.submit([]{return 1;}));
    futures.add_back(spc.submit([]{return 2;}));

    futures.wait();
    auto res = futures.get();
    for (auto& each: res) {
        std::cout<<"got "<<each<<std::endl;
    }
}