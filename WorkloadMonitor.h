#pragma once

#include <atomic>
#include <thread>
#include <deque>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>

enum class WorkloadState { STABLE, READ_HEAVY, WRITE_HEAVY };

class WorkloadMonitor {
private:
    std::atomic<uint64_t> read_ops{0};
    std::atomic<uint64_t> write_ops{0};

    std::deque<double> history;
    WorkloadState current_state{WorkloadState::STABLE};
    std::mutex state_mutex;

    const double T_HIGH = 0.8;
    const double T_LOW = 0.4;
    const int W = 5; 
    const int K = 3; 
    
    // 🔥 TASK 5.2: Stability Hysteresis (10 Seconds)
    const int MIN_RESIDENCE_TIME_SEC = 10;
    std::chrono::steady_clock::time_point last_transition_time;

    std::atomic<bool> running{true};
    std::thread background_thread;
    int window_ms;

    void observer_loop() {
        last_transition_time = std::chrono::steady_clock::now();
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(window_ms));
            if (!running) break;

            uint64_t current_reads = read_ops.exchange(0);
            uint64_t current_writes = write_ops.exchange(0);
            uint64_t total = current_reads + current_writes;

            double ratio = 0.0;
            if (total > 0) ratio = static_cast<double>(current_reads) / total;

            history.push_front(ratio);
            if (history.size() > W) history.pop_back();

            int high_count = 0;
            for (double r : history) {
                if (r >= T_HIGH) high_count++;
            }

            std::lock_guard<std::mutex> lock(state_mutex);
            auto now = std::chrono::steady_clock::now();
            auto time_in_state = std::chrono::duration_cast<std::chrono::seconds>(now - last_transition_time).count();

            // Block transitions if we haven't met the minimum residence time
            if (time_in_state < MIN_RESIDENCE_TIME_SEC && current_state != WorkloadState::STABLE) {
                continue; 
            }

            if (current_state != WorkloadState::READ_HEAVY && high_count >= K) {
                current_state = WorkloadState::READ_HEAVY;
                last_transition_time = now;
                std::cout << "\n[BRAIN] 🔥 MIGRATION TRIGGERED! Workload is Read-Heavy.\n";
            } 
            else if (current_state == WorkloadState::READ_HEAVY && ratio < T_LOW) {
                current_state = WorkloadState::WRITE_HEAVY;
                last_transition_time = now;
                std::cout << "\n[BRAIN] 📉 REVERTING. Workload became Write-Heavy. Dropping Index.\n";
                history.assign(W, 0.0); 
            }
        }
    }

public:
    WorkloadMonitor(int window_duration_ms = 2000) : window_ms(window_duration_ms) {
        background_thread = std::thread(&WorkloadMonitor::observer_loop, this);
    }

    ~WorkloadMonitor() {
        running = false;
        if (background_thread.joinable()) background_thread.join();
    }

    inline void record_read() { read_ops.fetch_add(1, std::memory_order_relaxed); }
    inline void record_write() { write_ops.fetch_add(1, std::memory_order_relaxed); }

    WorkloadState get_state() {
        std::lock_guard<std::mutex> lock(state_mutex);
        return current_state;
    }
};
