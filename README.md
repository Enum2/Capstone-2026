# Dual Indexing Database System (RocksDB + LMDB)

## 1. Overview

This project implements a hybrid database system that combines two storage engines:

* RocksDB (LSM-tree based) for efficient write-heavy workloads
* LMDB (B+ tree based) for efficient read-heavy workloads

The system dynamically adapts to workload patterns using a monitoring and decision layer, aiming to balance write throughput and read latency.

---

## 2. Objectives

* Reduce write amplification in write-heavy scenarios
* Improve read latency in read-heavy scenarios
* Provide a unified interface over two fundamentally different storage engines
* Enable workload-aware adaptive behavior

---

## 3. Project Structure

### Core Components

* `StorageManager.h`
  Handles interaction with both RocksDB and LMDB. Provides unified APIs for read/write operations.

* `WorkloadMonitor.h`
  Tracks runtime workload characteristics such as read/write ratio and access frequency.

---

### Engine and System Logic

* `adaptive_db/`
  Contains logic for adapting database behavior based on workload patterns.

* `hybrid_engine/`
  Implements the decision-making layer that determines where data should be stored or migrated.

---

### Baseline and Comparison

* `baseline_rocksdb/`
  Pure RocksDB implementation used as a baseline for comparison.

* `compare_db/`
  Utilities for comparing different database strategies and configurations.

* `compare_baseline.cpp`
  Compares baseline RocksDB performance against the hybrid system.

---

### Benchmarking and Profiling

* `benchmark/`
  Benchmark framework for evaluating system performance.

* `benchmark2.cpp`
  Extended benchmarking scenarios.

* `concurrency_profiler.cpp`
  Measures system behavior under concurrent workloads.

* `profiler/`
  Additional profiling utilities.

---

### Testing and Stress

* `test_monitor.cpp`
  Tests correctness of workload monitoring logic.

* `sprint5/`
  Contains experimental or development-stage implementations.

* `sprint5_stress.cpp`
  Stress testing under high-load conditions.

---

### Entry Point

* `main.cpp`
  Main driver program that initializes and runs the hybrid database system.

---

### Output Files

* `telemetry.csv`
  Stores runtime metrics such as latency, throughput, and system behavior.

* `workload_trace.csv`
  Logs workload patterns for analysis.

---

### Ignored Runtime Data

The following directories are generated at runtime and excluded using `.gitignore`:

* `data_lmdb/`
* `data_rocks/`
* `lmdb_ts/`
* `rocks_ts/`

These contain database files and logs and should not be committed.

---

## 4. Prerequisites

* Linux / WSL environment
* g++ with C++17 support
* RocksDB installed
* LMDB installed

---

## 5. Installation

### Install dependencies (Ubuntu/WSL)

```bash
sudo apt update
sudo apt install librocksdb-dev liblmdb-dev
```

---

## 6. Compilation

### Compile main system

```bash
g++ -std=c++17 main.cpp -o db_system \
    -lrocksdb -llmdb -lpthread
```

---

## 7. Execution

```bash
./db_system
```

---

## 8. Running Benchmarks and compare it with rockDB

```bash
g++ -std=c++17 compare_baseline.cpp -o benchmark \
    -lrocksdb -llmdb -lpthread

./benchmark
```

---

## 9. Running Stress Tests

```bash
g++ -std=c++17 sprint5_stress.cpp -o stress \
    -lrocksdb -llmdb -lpthread

./stress
```

---

## 10. System Workflow

1. Data is initially written to RocksDB for high write throughput
2. The workload monitor continuously tracks access patterns
3. Frequently accessed data is identified
4. The hybrid engine may migrate hot data to LMDB for faster reads
5. Telemetry and workload logs are generated for analysis

---

## 11. Notes

* Database files are large and automatically generated; they are excluded from version control
* Ensure sufficient disk space before running benchmarks or stress tests
* Performance depends on workload distribution and system configuration

---

## 12. Future Improvements

* Cost-based adaptive switching model
* Machine learning-based workload prediction
* Automated migration policies
* Distributed extension of the hybrid system

---

## 13. Author

* Your Name

---

## 14. License

This project is intended for academic and research purposes.
