# Dual Indexing Database System (RocksDB + LMDB)

## 1. Overview

This project implements a hybrid database system that combines:

* RocksDB (LSM-tree) for high write throughput
* LMDB (B+ tree) for efficient read operations

The system is designed to analyze workload patterns and optimize performance by leveraging the strengths of both storage engines.

---

## 2. Key Idea

Modern databases face a trade-off:

* Write-optimized systems (LSM trees)
* Read-optimized systems (B+ trees)

This project provides a unified system that:

* Monitors workload behavior
* Adapts storage strategy
* Enables comparative evaluation of performance

---

## 2.5 Research Paper

This project is accompanied by a detailed research paper that explains the system design, methodology, and experimental evaluation.

The paper covers:

* The motivation behind combining LSM-tree and B+ tree architectures
* Design of the dual indexing system
* Workload-aware adaptation strategy
* Performance evaluation and benchmarking results
* Trade-off analysis between write and read optimization

### Access the Paper

You can view the full research paper here:

[Download Research Paper](./researchPaper.pdf)

### Citation

If you use this work in your research, please cite it as:

```
Author(s), "Dual Indexing Database System: Balancing Write and Read Performance 
Using RocksDB and LMDB", 2026.
```

### Notes

* The paper is written in IEEE format
* It includes system architecture, experimental results, and analysis
* Recommended for understanding the theoretical foundation of this project


## 3. Repository Structure

### Core Headers

* `StorageManager.h`
  Provides abstraction for database operations across different storage backends.

* `WorkloadMonitor.h`
  Tracks workload characteristics such as read/write distribution.

---

### Main Execution

* `main.cpp`
  Entry point of the system. Initializes components and runs the database workflow.

---

### Benchmarking and Evaluation

* `benchmark2.cpp`
  Executes benchmark scenarios to evaluate performance.

* `compare_baseline.cpp`
  Compares hybrid system behavior with baseline implementations.

---

### Profiling and Analysis

* `concurrency_profiler.cpp`
  Evaluates performance under concurrent workloads.

---

### Testing

* `test_monitor.cpp`
  Validates workload monitoring functionality.

---

### Stress Testing

* `sprint5_stress.cpp`
  Simulates heavy workload conditions to test system stability.

---

### Baseline Implementation

* `baseline_rocksdb/`
  Contains standalone RocksDB-based implementation for comparison.

---

### Output Data

* `telemetry.csv`
  Stores runtime metrics such as latency and throughput.

* `workload_trace.csv`
  Logs workload patterns for analysis.

---

### Configuration

* `.gitignore`
  Excludes generated database files and logs.

---

## 4. Prerequisites

* Linux / WSL environment
* g++ compiler with C++17 support
* RocksDB installed
* LMDB installed

---

## 5. Installation

### Install dependencies

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

## 7. Running the System

```bash
./db_system
```

---

## 8. Running Benchmarks it will generate the baseline result

```bash
g++ -std=c++17 benchmark2.cpp -o benchmark \
    -lrocksdb -llmdb -lpthread

./benchmark
```

---
## 8. Running Benchmarks it will compare the result with baseline

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

## 10. Running Profiling

```bash
g++ -std=c++17 concurrency_profiler.cpp -o profiler \
    -lrocksdb -llmdb -lpthread

./profiler
```

---

## 11. System Workflow

1. Data is written to the storage layer
2. WorkloadMonitor tracks access patterns
3. Performance metrics are recorded in telemetry.csv
4. Benchmark and profiling tools analyze system behavior

---

## 12. Notes

* Database files are generated at runtime and are excluded from version control
* Ensure sufficient disk space before running large benchmarks
* Performance depends on workload characteristics

---

## 13. Future Work

* Adaptive switching between storage engines
* Cost-based optimization
* Machine learning-based workload prediction
* Distributed system extension

---

## 14. Author

* Your Name

---

## 15. License

This project is intended for academic and research purposes.
