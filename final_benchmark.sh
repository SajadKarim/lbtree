#!/bin/bash

# Final LBTree Benchmark Script
# Uses confirmed working parameters and your specified random number generation

set -e

echo "=========================================="
echo "         LBTree Performance Benchmark"
echo "=========================================="

# Configuration
NUM_RECORDS=${1:-1000000}  # Default 1M, can be overridden
THREAD_NUM=1
MEMPOOL_SIZE=100  # MB - confirmed working
NVMPOOL_SIZE=100  # MB - confirmed working
NVM_FILE="/tmp/lbtree_benchmark.nvm"

echo "Configuration:"
echo "  Records: $NUM_RECORDS"
echo "  Threads: $THREAD_NUM"
echo "  Memory Pool: ${MEMPOOL_SIZE} MB"
echo "  NVM Pool: ${NVMPOOL_SIZE} MB"

# Clean up
echo ""
echo "Cleaning up previous files..."
rm -f insert_keys.bin search_keys.bin delete_keys.bin
rm -f $NVM_FILE
rm -f temp_generator

# Create data generator with the specified number of records
echo ""
echo "Creating data generator..."
cat > temp_generator.cpp << EOF
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <cstdint>

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \\
    __VECTOR__.resize(__END__ - __START__); \\
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \\
    std::random_device _rd; \\
    std::mt19937 _eng(_rd()); \\
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \\
}

int main() {
    const size_t NUM_RECORDS = $NUM_RECORDS;
    
    std::cout << "Generating " << NUM_RECORDS << " random uint64_t records using your specified macro..." << std::endl;
    
    std::vector<uint64_t> data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
    
    // Write insert keys
    std::ofstream insert_file("insert_keys.bin", std::ios::binary);
    insert_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    insert_file.close();
    
    // Write search keys (same as insert keys)
    std::ofstream search_file("search_keys.bin", std::ios::binary);
    search_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    search_file.close();
    
    // Create delete keys (first half, shuffled)
    std::vector<uint64_t> delete_keys(data.begin(), data.begin() + NUM_RECORDS / 2);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(delete_keys.begin(), delete_keys.end(), g);
    
    std::ofstream delete_file("delete_keys.bin", std::ios::binary);
    delete_file.write(reinterpret_cast<const char*>(delete_keys.data()), delete_keys.size() * sizeof(uint64_t));
    delete_file.close();
    
    std::cout << "Data files created successfully!" << std::endl;
    return 0;
}
EOF

# Build and run data generator
echo "Building and running data generator..."
g++ -O3 -std=c++11 -o temp_generator temp_generator.cpp
./temp_generator

# Build lbtree
echo ""
echo "Building lbtree..."
make clean > /dev/null 2>&1
make lbtree > /dev/null 2>&1

echo ""
echo "=========================================="
echo "         Running Performance Tests"
echo "=========================================="

# INSERT Benchmark
echo ""
echo "=== INSERT BENCHMARK ==="
echo "Inserting $NUM_RECORDS records..."
rm -f $NVM_FILE  # Clean start
echo "Command: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $NUM_RECORDS insert_keys.bin"
echo ""
/usr/bin/time -f "INSERT PERFORMANCE: %e seconds elapsed, %M KB max memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $NUM_RECORDS insert_keys.bin
echo ""
echo "Insert benchmark completed!"

# SEARCH Benchmark
echo ""
echo "=== SEARCH BENCHMARK ==="
echo "Searching $NUM_RECORDS records..."
echo "Command: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin"
echo ""
/usr/bin/time -f "SEARCH PERFORMANCE: %e seconds elapsed, %M KB max memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin
echo ""
echo "Search benchmark completed!"

# DELETE Benchmark
DELETE_RECORDS=$((NUM_RECORDS / 2))
echo ""
echo "=== DELETE BENCHMARK ==="
echo "Deleting $DELETE_RECORDS records..."
echo "Command: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin"
echo ""
/usr/bin/time -f "DELETE PERFORMANCE: %e seconds elapsed, %M KB max memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin
echo ""
echo "Delete benchmark completed!"

echo ""
echo "=========================================="
echo "            BENCHMARK SUMMARY"
echo "=========================================="
echo "Successfully benchmarked LBTree with:"
echo "  • INSERT: $NUM_RECORDS uint64_t records"
echo "  • SEARCH: $NUM_RECORDS uint64_t records"  
echo "  • DELETE: $DELETE_RECORDS uint64_t records"
echo ""
echo "Data generation used your specified macro:"
echo "  GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__)"
echo ""
echo "Performance results are shown above."
echo "All operations completed successfully!"

# Save results
cat > lbtree_benchmark_results.txt << EOF
LBTree Benchmark Results
========================
Date: $(date)
Records: $NUM_RECORDS uint64_t values
Operations: INSERT, SEARCH, DELETE
Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro (shuffled 1 to $NUM_RECORDS)

Configuration:
- Threads: $THREAD_NUM
- Memory Pool: ${MEMPOOL_SIZE} MB  
- NVM Pool: ${NVMPOOL_SIZE} MB

All operations completed successfully.
Detailed performance timing is available in console output.
EOF

echo ""
echo "Results saved to lbtree_benchmark_results.txt"

# Cleanup
echo ""
echo "Cleaning up temporary files..."
rm -f temp_generator temp_generator.cpp
rm -f insert_keys.bin search_keys.bin delete_keys.bin
echo "Cleanup completed."

echo ""
echo "Benchmark finished successfully!"