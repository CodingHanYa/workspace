#include <workspace/workspace.h>

int main() {
    // 1 threads
    wsp::workbranch br;

    // normal task 
    br.submit<wsp::task::nor>([]{ std::cout<<"task B done\n";});
    
    // urgent task
    br.submit<wsp::task::urg>([]{ std::cout<<"task A done\n";});

    // wait for tasks done (timeout: no limit)
    br.wait_tasks();
}