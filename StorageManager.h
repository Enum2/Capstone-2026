#pragma once

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/cache.h>
#include <lmdb.h>
#include "WorkloadMonitor.h"

#include <string>
#include <atomic>
#include <thread>
#include <iostream>
#include <fstream>
#include <shared_mutex>
#include <filesystem>
#include <cstring>
#include <chrono>

class StorageManager {
private:
    rocksdb::DB* rocks_db;
    MDB_env* lmdb_env;
    MDB_dbi lmdb_dbi;

    WorkloadMonitor monitor;

    std::atomic<bool> is_lmdb_ready{false};
    std::atomic<bool> is_migrating{false};
    std::shared_mutex index_rw_lock; 


    std::ofstream telemetry_csv;
    std::mutex telemetry_mutex;
    std::chrono::steady_clock::time_point start_time;  


    bool check_ram_safety() {
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        long total_mem = 0, available_mem = 0;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) sscanf(line.c_str(), "MemTotal: %ld kB", &total_mem);
            if (line.find("MemAvailable:") == 0) sscanf(line.c_str(), "MemAvailable: %ld kB", &available_mem);
        }
        if (total_mem == 0) return true;
        
        double used_percent = 100.0 * (1.0 - ((double)available_mem / total_mem));
        return used_percent < 90.0;
    }

    void build_lmdb_index() {
        if (is_migrating) return;
        
        if (!check_ram_safety()) {
            std::cout << "[RAM GUARD]  Aborting Migration! System RAM usage is >90%.\n";
            return;
        }
        
        std::cout << "\n[MIGRATION]  Phase 1: Snapping RocksDB state...\n";
        auto mig_start = std::chrono::high_resolution_clock::now();
        const rocksdb::Snapshot* snapshot = rocks_db->GetSnapshot();
        
        is_migrating = true; 
        std::cout << "[MIGRATION]  Phase 2: Dual-write enabled. Building index...\n";

        rocksdb::ReadOptions read_opts;
        read_opts.snapshot = snapshot;
        rocksdb::Iterator* it = rocks_db->NewIterator(read_opts);

        MDB_txn* txn;
        mdb_txn_begin(lmdb_env, NULL, 0, &txn);
        
        int count = 0;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            MDB_val k, v;
            k.mv_size = it->key().size();
            k.mv_data = (void*)it->key().data();
            v.mv_size = it->value().size();
            v.mv_data = (void*)it->value().data();

            mdb_put(txn, lmdb_dbi, &k, &v, MDB_NOOVERWRITE);

            count++;
            if (count % 10000 == 0) {
                mdb_txn_commit(txn);
                mdb_txn_begin(lmdb_env, NULL, 0, &txn);
            }
        }
        mdb_txn_commit(txn);
        delete it;
        rocks_db->ReleaseSnapshot(snapshot);

        is_lmdb_ready = true;
        is_migrating = false;
        
        auto mig_end = std::chrono::high_resolution_clock::now();
        double mig_time = std::chrono::duration_cast<std::chrono::milliseconds>(mig_end - mig_start).count();
        std::cout << "[MIGRATION]  Phase 3: LMDB Built (" << count << " keys) in " << mig_time << " ms.\n";
    }

    void drop_lmdb_index() {
        if (!is_lmdb_ready) return;
        std::unique_lock<std::shared_mutex> lock(index_rw_lock);
        is_lmdb_ready = false;
        is_migrating = false;
        std::cout << "[SYSTEM]  Dropping LMDB Index to save RAM.\n";
        
        MDB_txn* txn;
        mdb_txn_begin(lmdb_env, NULL, 0, &txn);
        mdb_drop(txn, lmdb_dbi, 0); 
        mdb_txn_commit(txn);
    }

public:
    StorageManager() : monitor(2000), start_time(std::chrono::steady_clock::now()) {  
        telemetry_csv.open("telemetry.csv");
        telemetry_csv << "Timestamp_ms,Engine,Latency_us,Operation\n";

        std::filesystem::create_directories("data_rocks");
        rocksdb::Options options;
        options.create_if_missing = true;
        options.write_buffer_size = 64 * 1024 * 1024; 
        rocksdb::BlockBasedTableOptions table_options;
        table_options.block_cache = rocksdb::NewLRUCache(128 * 1024 * 1024); 
        table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10));
        options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        rocksdb::DB::Open(options, "data_rocks", &rocks_db);

        std::filesystem::create_directories("data_lmdb");
        mdb_env_create(&lmdb_env);
        mdb_env_set_mapsize(lmdb_env, 1ULL * 1024 * 1024 * 1024); 
        mdb_env_open(lmdb_env, "./data_lmdb", MDB_NOSYNC, 0664);
        MDB_txn* txn;
        mdb_txn_begin(lmdb_env, NULL, 0, &txn);
        mdb_dbi_open(txn, NULL, MDB_CREATE, &lmdb_dbi);
        mdb_txn_commit(txn);
    }

    ~StorageManager() {
        telemetry_csv.close();
        delete rocks_db;
        mdb_dbi_close(lmdb_env, lmdb_dbi);
        mdb_env_close(lmdb_env);
    }

    void log_telemetry(const std::string& engine, double latency_us, const std::string& op) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        std::lock_guard<std::mutex> lock(telemetry_mutex);
        telemetry_csv << elapsed_ms << "," << engine << "," << latency_us << "," << op << "\n";
    }

    void Put(const std::string& key, const std::string& value) {
        monitor.record_write();
        auto t1 = std::chrono::high_resolution_clock::now();
        
        rocks_db->Put(rocksdb::WriteOptions(), key, value);
        if (is_lmdb_ready || is_migrating) {
            std::shared_lock<std::shared_mutex> lock(index_rw_lock);
            if (is_lmdb_ready || is_migrating) { 
                MDB_txn* txn;
                mdb_txn_begin(lmdb_env, NULL, 0, &txn);
                MDB_val k, v;
                k.mv_size = key.size(); k.mv_data = (void*)key.data();
                v.mv_size = value.size(); v.mv_data = (void*)value.data();
                mdb_put(txn, lmdb_dbi, &k, &v, 0); 
                mdb_txn_commit(txn);
            }
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        if (rand() % 100 == 0)
            log_telemetry("RocksDB+Dual", std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count(), "PUT");
    }

    void Scan(const std::string& start_key, const std::string& end_key) {
        monitor.record_read();
        WorkloadState state = monitor.get_state();

        if (state == WorkloadState::READ_HEAVY && !is_lmdb_ready && !is_migrating) {
            std::thread(&StorageManager::build_lmdb_index, this).detach();
        } 
        else if (state == WorkloadState::WRITE_HEAVY && is_lmdb_ready) {
            drop_lmdb_index();
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        std::string active_engine = "RocksDB";

        if (is_lmdb_ready) {
            std::shared_lock<std::shared_mutex> lock(index_rw_lock);
            if (is_lmdb_ready) { 
                active_engine = "LMDB";
                MDB_txn* txn;
                mdb_txn_begin(lmdb_env, NULL, MDB_RDONLY, &txn);
                MDB_cursor* cursor;
                mdb_cursor_open(txn, lmdb_dbi, &cursor);
                MDB_val k, v;
                k.mv_size = start_key.size(); k.mv_data = (void*)start_key.data();
                if (mdb_cursor_get(cursor, &k, &v, MDB_SET_RANGE) == MDB_SUCCESS) {
                    do {
                        if (std::memcmp(k.mv_data, end_key.data(), std::min(k.mv_size, end_key.size())) > 0) break;
                        volatile size_t s = v.mv_size; 
                    } while (mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == MDB_SUCCESS);
                }
                mdb_cursor_close(cursor);
                mdb_txn_abort(txn);
                goto end_scan;
            }
        }

        {
            rocksdb::Iterator* it = rocks_db->NewIterator(rocksdb::ReadOptions());
            for (it->Seek(start_key); it->Valid() && it->key().compare(end_key) <= 0; it->Next()) {
                volatile auto val = it->value();
            }
            delete it;
        }

    end_scan:
        auto t2 = std::chrono::high_resolution_clock::now();
        if (rand() % 10 == 0)
            log_telemetry(active_engine, std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count(), "SCAN");
    }
};
