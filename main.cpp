#include "StorageManager.h"
#include <rocksdb/db.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <filesystem>
#include <thread>

using namespace std;
using namespace std::chrono;

// Helper to format keys nicely
string format_key(int i) {
    stringstream ss;
    ss << "key_" << setw(7) << setfill('0') << i;
    return ss.str();
}

enum OpType { SCAN, WRITE };
struct Operation {
    OpType type;
    string key_start;
    string key_end; 
    string write_val; 
};

// Generates the deterministic workload
vector<Operation> generate_workload(int num_ops) {
    vector<Operation> workload;
    workload.reserve(num_ops);
    mt19937 rng(42); 
    uniform_int_distribution<int> ratio_dist(1, 100);
    uniform_int_distribution<int> key_dist(1, 990000); 

    for (int i = 0; i < num_ops; i++) {
        Operation op;
        int k = key_dist(rng);
        if (ratio_dist(rng) <= 85) { 
            op.type = SCAN;
            op.key_start = format_key(k);
            op.key_end = format_key(k + 1000); // 1,000 Key Scan
        } else { 
            op.type = WRITE;
            op.key_start = format_key(k);
            op.write_val = "mixed_workload_update";
        }
        workload.push_back(op);
    }
    return workload;
}

int main() {
    cout << "========================================================\n";
    cout << " HEAD-TO-HEAD: PURE ROCKSDB vs ADAPTIVE ENGINE\n";
    cout << "========================================================\n";

    cout << "[SYSTEM] Cleaning up old database files from hard drive...\n";
    std::filesystem::remove_all("baseline_rocksdb");
    std::filesystem::remove_all("data_rocks");
    std::filesystem::remove_all("data_lmdb");

    // SETTINGS: 1 Million Keys, 1 Lakh Ops
    int initial_keys = 1000000;
    int workload_ops = 100000; 
    
    cout << "[SYSTEM] Generating 1 Lakh Operations Workload...\n";
    auto workload = generate_workload(workload_ops);

    // ---------------------------------------------------------
    // TEST 1: PURE ROCKSDB (THE BASELINE)
    // ---------------------------------------------------------
    cout << "\n[TEST 1] Booting Pure RocksDB (Baseline)...\n";
    rocksdb::DB* raw_db;
    rocksdb::Options opt;
    opt.create_if_missing = true;
    rocksdb::DB::Open(opt, "baseline_rocksdb", &raw_db);

    cout << "Ingesting exactly 1,000,000 keys (This will take a moment)...\n";
    for (int i = 0; i < initial_keys; i++) {
        raw_db->Put(rocksdb::WriteOptions(), format_key(i), "initial_data");
        if (i > 0 && i % 200000 == 0) cout << "  ... " << i << " / 1000000 keys ingested\n";
    }

    cout << "Running Mixed Workload (85% Scans / 15% Writes)...\n";
    auto start_baseline = high_resolution_clock::now();
    int progress_1 = 0;
    
    for (const auto& op : workload) {
        if (op.type == SCAN) {
            rocksdb::Iterator* it = raw_db->NewIterator(rocksdb::ReadOptions());
            for (it->Seek(op.key_start); it->Valid() && it->key().compare(op.key_end) <= 0; it->Next()) {
                volatile auto val = it->value();
            }
            delete it;
        } else {
            raw_db->Put(rocksdb::WriteOptions(), op.key_start, op.write_val);
        }
        
        progress_1++;
        if (progress_1 % 20000 == 0) {
            cout << "  ... " << progress_1 << " / " << workload_ops << " ops completed\n";
        }
    }
    
    auto end_baseline = high_resolution_clock::now();
    double baseline_ms = duration_cast<milliseconds>(end_baseline - start_baseline).count();
    cout << " Pure RocksDB Total Time: " << baseline_ms << " ms\n";
    delete raw_db;

    // ---------------------------------------------------------
    // TEST 2: THE ADAPTIVE ENGINE (OUR METHOD)
    // ---------------------------------------------------------
    cout << "\n[TEST 2] Booting Adaptive Engine...\n";
    StorageManager manager;

    cout << "Ingesting exactly 1,000,000 keys...\n";
    for (int i = 0; i < initial_keys; i++) {
        manager.Put(format_key(i), "initial_data");
        if (i > 0 && i % 200000 == 0) cout << "  ... " << i << " / 1000000 keys ingested\n";
    }

    cout << "Running Exact Same Mixed Workload (85% Scans / 15% Writes)...\n";
    auto start_adaptive = high_resolution_clock::now();
    int progress_2 = 0;
    
    for (const auto& op : workload) {
        if (op.type == SCAN) {
            manager.Scan(op.key_start, op.key_end);
        } else {
            manager.Put(op.key_start, op.write_val);
        }
        
        progress_2++;
        if (progress_2 % 20000 == 0) {
            cout << "  ... " << progress_2 << " / " << workload_ops << " ops completed\n";
        }
        
        //  The artificial sleep_for has been REMOVED! Full speed ahead!
    }
    
    auto end_adaptive = high_resolution_clock::now();
    double adaptive_ms = duration_cast<milliseconds>(end_adaptive - start_adaptive).count();
    
    cout << "Adaptive Engine Total Time: " << adaptive_ms << " ms\n";

    // ---------------------------------------------------------
    // THE FINAL VERDICT
    // ---------------------------------------------------------
    cout << "\n========================================================\n";
    cout << " FINAL VERDICT\n";
    cout << "========================================================\n";
    cout << "Pure RocksDB:    " << baseline_ms << " ms\n";
    cout << "Adaptive Engine: " << adaptive_ms << " ms\n";
    
    if (adaptive_ms < baseline_ms) {
        double saved = baseline_ms - adaptive_ms;
        double percentage = (saved / baseline_ms) * 100.0;
        cout << " Your method won by " << saved << " ms (" << fixed << setprecision(1) << percentage << "% faster overall!)\n";
    } else {
        cout << "RocksDB won. (The migration cost of 1 Million keys was too heavy to pay off in just 1 Lakh operations).\n";
    }
    cout << "========================================================\n";

    return 0;
}
