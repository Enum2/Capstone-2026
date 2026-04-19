#include "StorageManager.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <filesystem>

using namespace std;
using namespace std::chrono;

string format_key(int i) {
    stringstream ss;
    ss << "key_" << setw(7) << setfill('0') << i;
    return ss.str();
}

int main() {
    cout << "========================================================\n";
    cout << "🌪️ SPRINT 5: 2-MINUTE ADAPTIVE STRESS TEST\n";
    cout << "========================================================\n";

    std::filesystem::remove_all("data_rocks");
    std::filesystem::remove_all("data_lmdb");
    std::filesystem::remove("telemetry.csv");

    StorageManager manager;

    // --- TASK 5.4: WARM-UP (1 MILLION KEYS) ---
    cout << "\n[PHASE 1] Warm-up: Ingesting 1,000,000 Keys...\n";
    auto start_ingest = high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        manager.Put(format_key(i), "massive_payload_data");
        if (i % 250000 == 0 && i > 0) cout << "  ... " << i << " keys written.\n";
    }
    auto end_ingest = high_resolution_clock::now();
    cout << "✅ 1M keys ingested in " << duration_cast<milliseconds>(end_ingest - start_ingest).count() << " ms.\n";

    // --- TASK 5.5: 2-MINUTE WORKLOAD SHIFT ---
    cout << "\n[PHASE 2] Starting 2-Minute Shifting Workload...\n";
    cout << "This will alternate between heavy writes and heavy reads, generating telemetry.\n";

    mt19937 rng(42);
    uniform_int_distribution<int> key_dist(10000, 900000);

    auto start_stress = high_resolution_clock::now();
    int seconds_elapsed = 0;

    // Run for exactly 120 seconds
    while (seconds_elapsed < 120) {
        seconds_elapsed = duration_cast<seconds>(high_resolution_clock::now() - start_stress).count();

        // Phase A: Write Heavy (0-30s and 60-90s)
        bool is_write_heavy = (seconds_elapsed < 30) || (seconds_elapsed >= 60 && seconds_elapsed < 90);

        if (is_write_heavy) {
            for(int i=0; i<50; i++) manager.Put(format_key(key_dist(rng)), "update");
            // Occasional read to keep background alive
            manager.Scan(format_key(key_dist(rng)), format_key(key_dist(rng) + 50));
        } 
        // Phase B: Read Heavy (30-60s and 90-120s)
        else {
            for(int i=0; i<50; i++) manager.Scan(format_key(key_dist(rng)), format_key(key_dist(rng) + 500));
            // Occasional write
            manager.Put(format_key(key_dist(rng)), "update");
        }

        if (seconds_elapsed % 10 == 0) {
            // Prevent spamming the console 1000s of times
            static int last_printed = -1;
            if (seconds_elapsed != last_printed) {
                cout << "⏱️ Elapsed: " << seconds_elapsed << "s / 120s (" 
                     << (is_write_heavy ? "Write-Heavy" : "Read-Heavy") << ")\n";
                last_printed = seconds_elapsed;
            }
        }
    }

    cout << "\n========================================================\n";
    cout << "✅ STRESS TEST COMPLETE.\n";
    cout << "💾 Data saved to 'telemetry.csv'. You can now graph the results!\n";
    cout << "========================================================\n";

    return 0;
}
