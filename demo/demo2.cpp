/**
 * How to adjust the dynamic thread pool.
 * This is just a reference.
*/
#include "../hipe.h"

bool closed = false;
uint tnumb = 8;
uint task_pack_numb = 5000;
uint milli_per_submit = 500; // 0.5s

void manager(hipe::DynamicThreadPond* pond) 
{   
    uint unit = 4;
    uint prev_load = 0;
    uint max_thread_numb = 200;
    uint min_thread_numb = 8;
    uint total = 0;

    while (!closed) 
    {   
        auto new_load = pond->resetTaskLoaded();
        auto tnumb = pond->getThreadNumb();
        auto task_remain = pond->getTasksRemain();
        total += new_load; 

        printf("threads: %-3d remain: %-4d loaded: %d\n", tnumb, task_remain, new_load);
        fflush(stdout);

        // if !prev_load,  we still try add threads and have a look at the perform
        if (new_load > prev_load)  
        {
            prev_load = new_load;
            if (tnumb < max_thread_numb) {
                pond->addThreads(unit);
            }
        } 
        if (!task_remain && tnumb > min_thread_numb) {
            pond->delThreads(unit);
        }

        // I think that the interval of each sleep should be an order of magnitude more than the time a task cost.
        hipe::util::sleep_for_seconds(1); // 1s
    }
    total += pond->resetTaskLoaded();
    hipe::util::print("total load ", total);
}

int main() 
{
    hipe::DynamicThreadPond pond(tnumb);

    // total 0.1s
    auto task1 = []{hipe::util::sleep_for_milli(20);};
    auto task2 = []{hipe::util::sleep_for_milli(30);};
    auto task3 = []{hipe::util::sleep_for_milli(50);}; 

    auto packs = (double)100/((double)milli_per_submit/1000);
    auto task_nums = packs * 3;
    hipe::util::print("Submit ", packs, " task pack and ", task_nums, " task per second");
    hipe::util::print("So we hope that the threads is able to load [", task_nums, "] task per second");
    hipe::util::print(hipe::util::boundary('=', 65));

    // create a manager thread.
    std::thread mger(manager, &pond);

    int count = task_pack_numb/100;
    while (count--) 
    {   
        for (int i = 0; i < 100; ++i) {
            pond.submit(task1);
            pond.submit(task2);
            pond.submit(task3);
        }
        hipe::util::sleep_for_milli(milli_per_submit);
    }

    // wait for task done and than close the manager
    pond.waitForTasks();
    closed = true;
    mger.join();

}