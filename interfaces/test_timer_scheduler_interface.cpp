#include <hipe/hipe.h>
#include <timer_scheduler/HeapTimerScheduler.h>
#include <timer_scheduler/RbtreeTimerScheduler.h>

using std::clog;
using std::cout;
using std::endl;
using std::chrono::seconds;

int main() {
    //     hipe::DynamicThreadPond pond{16};
    //     hipe::TimerScheduler<hipe::DynamicThreadPond>* ts = new
    //     hipe::HeapTimerScheduler<hipe::DynamicThreadPond>(&pond);

    hipe::SteadyThreadPond pond{16, 16};
    hipe::TimerScheduler<hipe::SteadyThreadPond>* ts = new hipe::RbtreeTimerScheduler<hipe::SteadyThreadPond>(&pond);

    ts->submit([]() { cout << "warning, iron curtain detected." << endl; });
    ts->submit([]() { cout << "warning, nuclear silo detected." << endl; });

    ts->start();

    size_t tid_1 = ts->submit([]() { cout << "iron curtain ready." << endl; }, seconds(1), seconds(1));
    size_t tid_2 = ts->submit([]() { cout << "nuclear missile ready." << endl; }, seconds(2), seconds(2));

    ts->submit(
        [&ts, tid_1]() {
            bool ret = ts->cancel(tid_1, false);
            if (ret) {
                clog << "cancel task 1 success" << endl;
            }
        },
        seconds(4), seconds(4));

    std::this_thread::sleep_for(seconds(8));
    bool ret = ts->cancel(tid_2, false);
    if (ret) {
        clog << "cancel task 2 success" << endl;
    }

    (*ts)(cout);
}