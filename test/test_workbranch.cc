#include <workspace/workspace.h>

int main() {

    wsp::workbranch br(2);

    // add a worker
    br.add_worker();
    std::cout<<"workers: "<<br.num_workers()<<std::endl;

    // delete a worker
    br.del_worker();
    br.del_worker();
    std::cout<<"workers: "<<br.num_workers()<<std::endl;

    // normal task
    br.submit([]{std::cout<<"<normal>"<<std::endl;}); 

    // normal task
    br.submit<wsp::task::nor>([]{std::cout<<"<normal>"<<std::endl;}); 

    // urgent task, executed as soon as possible
    br.submit<wsp::task::urg>([]{std::cout<<"<urgent>"<<std::endl;});   // 

    // sequence task
    br.submit<wsp::task::seq>([]{std::cout<<"<sequence1>"<<std::endl;},
                              []{std::cout<<"<sequence2>"<<std::endl;},
                              []{std::cout<<"<sequence3>"<<std::endl;});
    // wait for tasks done
    br.wait_tasks(); 
}
