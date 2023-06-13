#include <workspace/workspace.h>

int main() {
    // 2 threads
    wsp::workbranch br(2);

    // no return
    br.submit([]{ std::cout<<"hello world"<<std::endl; });  

    // return std::future<int>
    auto result = br.submit([]{ return 2023; });  
    std::cout<<"Got "<<result.get()<<std::endl;   

    // wait for tasks done (timeout: 1000 milliseconds)
    br.wait_tasks(1000); 
}