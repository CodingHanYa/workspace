#include <chrono>
#include <iomanip>
#include <mutex>
#include <vector>

#include "workspace/workspace.hpp"
#include "timewait.h"

int main(int argn, char** argvs) {
    int task_nums, thread_nums;
    if (argn == 3) {
        thread_nums = atoi(argvs[1]);
        task_nums = atoi(argvs[2]);
    } else {
        fprintf(stderr, "Invalid parameter! usage: [threads + tasks]\n");
        return -1;
    }

    for (auto strategy : {wsp::WaitStrategy::LowLatencyMode, wsp::WaitStrategy::BalancedMode, wsp::WaitStrategy::SleepMode}) {
        wsp::workbranch wb(thread_nums, strategy);
        std::vector<long long> latencies;
        latencies.reserve(task_nums);
        std::mutex latency_mutex;

        auto task = [&latencies, &latency_mutex](std::chrono::steady_clock::time_point submit_time) {
            auto start_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(start_time - submit_time).count();
            {
                std::lock_guard<std::mutex> lock(latency_mutex);
                latencies.push_back(latency);
            }
        };

        // record submit_time
        for (int i = 0; i < task_nums; ++i) {
            auto submit_time = std::chrono::steady_clock::now();
            wb.submit([=]() { task(submit_time); });
        }

        wb.wait_tasks();

        // calculate latency
        long long total_latency = 0;
        long long max_latency = 0;
        long long min_latency = latencies.empty() ? 0 : latencies[0];

        for (auto latency : latencies) {
            total_latency += latency;
            if (latency > max_latency) {
                max_latency = latency;
            }
            if (latency < min_latency) {
                min_latency = latency;
            }
        }

        double avg_latency = latencies.empty() ? 0.0 : static_cast<double>(total_latency) / latencies.size();

        const char* strategy_name = "";
        switch (strategy) {
            case wsp::WaitStrategy::LowLatencyMode:
                strategy_name = "LowLatencyMode";
                break;
            case wsp::WaitStrategy::BalancedMode:
                strategy_name = "BalancedMode";
                break;
            case wsp::WaitStrategy::SleepMode:
                strategy_name = "SleepMode";
                break;
        }

        std::cout << "Strategy: " << std::left << std::setw(15) << strategy_name
                  << " | Threads: " << std::setw(2) << thread_nums << " | Tasks: " << std::setw(8) << task_nums
                  << " | Avg Latency: " << std::setw(8) << std::right << std::fixed << std::setprecision(2)
                  << avg_latency << " us"
                  << " | Min Latency: " << std::setw(4) << min_latency << " us"
                  << " | Max Latency: " << std::setw(8) << max_latency << " us" << std::endl;
    }

    return 0;
}
