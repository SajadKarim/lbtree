#!/bin/bash

# Working LBTree Benchmark Script
# Based on the exact parameters that were tested and confirmed working

set -e

echo "=========================================="
echo "         LBTree Working Benchmark"
echo "=========================================="

# Configuration - using parameters that were confirmed to work
NUM_RECORDS=${1:-1000000}  # Default 1M, can be overridden with first argument
THREAD_NUM=1
MEMPOOL_SIZE=100  # MB - confirmed working size
NVMPOOL_SIZE=100  # MB - confirmed working size
NVM_FILE="/tmp/lbtree_working.nvm"

echo "Configuration:"
echo "  Records: $NUM_RECORDS"
echo "  Threads: $THREAD_NUM"
echo "  Memory Pool: ${MEMPOOL_SIZE} MB"
echo "  NVM Pool: ${NVMPOOL_SIZE} MB"
echo "  NVM File: $NVM_FILE"

# Clean up previous files
echo ""
echo "Cleaning up previous benchmark files..."
rm -f insert_keys.bin search_keys.bin delete_keys.bin
rm -f $NVM_FILE
rm -f generate_benchmark_data

# Step 1: Build and run data generator
echo ""
echo "Generating benchmark data..."
g++ -O3 -std=c++11 -o generate_benchmark_data -DNUM_RECORDS=$NUM_RECORDS - << 'EOF'
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <cstdint>

#ifndef NUM_RECORDS
#define NUM_RECORDS 1000000
#endif

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

int main() {
    const size_t num_records = NUM_RECORDS;
    
    std::cout << "Generating " << num_records << " random uint64_t records..." << std::endl;
    
    std::vector<uint64_t> data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, num_records + 1, data);
    
    // Write insert keys
    std::ofstream insert_file("insert_keys.bin", std::ios::binary);
    insert_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    insert_file.close();
    
    // Write search keys (same as insert keys)
    std::ofstream search_file("search_keys.bin", std::ios::binary);
    search_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    search_file.close();
    
    // Create delete keys (first half)
    std::vector<uint64_t> delete_keys(data.begin(), data.begin() + num_records / 2);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(delete_keys.begin(), delete_keys.end(), g);
    
    std::ofstream delete_file("delete_keys.bin", std::ios::binary);
    delete_file.write(reinterpret_cast<const char*>(delete_keys.data()), delete_keys.size() * sizeof(uint64_t));
    delete_file.close();
    
    std::cout << "Data generation completed!" << std::endl;
    std::cout << "Files created:" << std::endl;
    std::cout << "  insert_keys.bin: " << data.size() << " keys" << std::endl;
    std::cout << "  search_keys.bin: " << data.size() << " keys" << std::endl;
    std::cout << "  delete_keys.bin: " << delete_keys.size() << " keys" << std::endl;
    
    return 0;
}
EOF

./generate_benchmark_data

# Step 2: Build lbtree
echo ""
echo "Building lbtree..."
make clean > /dev/null 2>&1
make lbtree > /dev/null 2>&1

# Step 3: Run benchmarks
echo ""
echo "=========================================="
echo "         Starting Benchmarks"
echo "=========================================="

# Function to run a benchmark and measure time
run_benchmark() {
    local operation=$1
    local records=$2
    local keyfile=$3
    
    echo ""
    echo "=== $operation BENCHMARK ==="
    echo "Processing $records records..."
    
    # Clean NVM file before each operation to avoid conflicts
    rm -f $NVM_FILE
    
    echo "Starting $operation operation..."
    /usr/bin/time -f "Time: %e seconds, Memory: %M KB" \
        ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE $operation $records $keyfile
    
    echo "$operation operation completed successfully!"
}

# Run insert benchmark
run_benchmark "insert" $NUM_RECORDS "insert_keys.bin"

# Run search benchmark (reuse the same NVM file with inserted data)
echo ""
echo "=== SEARCH BENCHMARK ==="
echo "Processing $NUM_RECORDS records..."
echo "Starting search operation..."
/usr/bin/time -f "Time: %e seconds, Memory: %M KB" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin
echo "Search operation completed successfully!"

# Run delete benchmark
DELETE_RECORDS=$((NUM_RECORDS / 2))
echo ""
echo "=== DELETE BENCHMARK ==="
echo "Processing $DELETE_RECORDS records..."
echo "Starting delete operation..."
/usr/bin/time -f "Time: %e seconds, Memory: %M KB" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin
echo "Delete operation completed successfully!"

echo ""
echo "=========================================="
echo "         Benchmark Summary"
echo "=========================================="
echo "Successfully completed benchmarks for:"
echo "  - INSERT: $NUM_RECORDS records"
echo "  - SEARCH: $NUM_RECORDS records"
echo "  - DELETE: $DELETE_RECORDS records"
echo ""
echo "All operations used your specified GENERATE_RANDOM_NUMBER_ARRAY macro"
echo "for generating shuffled sequences of uint64_t values."

# Create results file
cat > lbtree_benchmark_results.txt << EOF
LBTree Benchmark Results
========================
Date: $(date)
Records processed: $NUM_RECORDS
Configuration:
- Memory Pool: ${MEMPOOL_SIZE} MB
- NVM Pool: ${NVMPOOL_SIZE} MB
- Threads: $THREAD_NUM

Operations completed successfully:
- INSERT: $NUM_RECORDS records
- SEARCH: $NUM_RECORDS records  
- DELETE: $DELETE_RECORDS records

Random number generation: Used GENERATE_RANDOM_NUMBER_ARRAY macro
Data type: uint64_t
Sequence: Shuffled values from 1 to $NUM_RECORDS

All timing information is available in the console output above.
EOF

echo ""
echo "Results summary saved to lbtree_benchmark_results.txt"

# Clean up
echo ""
echo "Cleaning up temporary files..."
rm -f generate_benchmark_data
rm -f insert_keys.bin search_keys.bin delete_keys.bin
echo "Cleanup completed."

echo ""
echo "=========================================="
echo "         Benchmark Completed Successfully!"
echo "=========================================="