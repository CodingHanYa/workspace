/* Created by kiki on 2023/2/4.
 **/

#include "../hipe.h"

using namespace hipe;

void multithread_wait_task()
{
	DynamicThreadPond pond (8);
	
	pond.submit([](){std::cout << "hello world!" << std::endl;});
	
	std::thread trd { [&pond](){ pond.waitForTasks(); } };
	pond.waitForTasks();
}

void poolthread_wait_task()
{
	DynamicThreadPond pond (8);
	pond.submit([&pond](){pond.waitForTasks(); });
	pond.waitForTasks();
}

int main()
{
	// multithread_wait_task();
	poolthread_wait_task();
}