#!/bin/bash

# Comprehensive LBTree Benchmark Script
# This script benchmarks insert, search, and delete operations using your specified random number generation

set -e

echo "=========================================="
echo "         LBTree Comprehensive Benchmark"
echo "=========================================="

# Configuration - you can modify these values
NUM_RECORDS=${1:-10000000}  # Default 10M, can be overridden with first argument
THREAD_NUM=1
MEMPOOL_SIZE=512  # MB
NVMPOOL_SIZE=512  # MB
NVM_FILE="/tmp/lbtree_benchmark.nvm"

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
rm -f lbtree_benchmark_results.txt
rm -f generate_benchmark_data test_small

# Step 1: Build the data generator
echo ""
echo "Building data generator..."
g++ -O3 -std=c++11 -o generate_benchmark_data generate_benchmark_data.cpp

# Step 2: Generate benchmark data
echo ""
echo "Generating benchmark data with your specified random number generation..."
echo "This may take a few minutes for 10M records..."

# Modify the data generator to use the specified number of records
sed "s/const size_t NUM_RECORDS = 10000000;/const size_t NUM_RECORDS = $NUM_RECORDS;/" generate_benchmark_data.cpp > temp_generator.cpp
g++ -O3 -std=c++11 -o temp_generator temp_generator.cpp
./temp_generator
rm -f temp_generator temp_generator.cpp

# Step 3: Build lbtree
echo ""
echo "Building lbtree..."
make clean > /dev/null 2>&1
make lbtree > /dev/null 2>&1

# Step 4: Run benchmarks
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
    echo "Command: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE $operation $records $keyfile"
    
    # Use time command to measure execution time
    echo "Starting $operation operation..."
    /usr/bin/time -f "Real time: %e seconds\nUser time: %U seconds\nSystem time: %S seconds\nMax memory: %M KB" \
        ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE $operation $records $keyfile
    
    echo "$operation operation completed!"
}

# Run insert benchmark
run_benchmark "insert" $NUM_RECORDS "insert_keys.bin"

# Run search benchmark
run_benchmark "lookup" $NUM_RECORDS "search_keys.bin"

# Run delete benchmark (half the records)
DELETE_RECORDS=$((NUM_RECORDS / 2))
run_benchmark "del" $DELETE_RECORDS "delete_keys.bin"

# Verify tree integrity after operations
echo ""
echo "=== TREE INTEGRITY CHECK ==="
echo "Checking tree structure after all operations..."
./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE check_tree

echo ""
echo "=========================================="
echo "         Benchmark Summary"
echo "=========================================="
echo "Successfully completed benchmarks for:"
echo "  - INSERT: $NUM_RECORDS records"
echo "  - SEARCH: $NUM_RECORDS records"
echo "  - DELETE: $DELETE_RECORDS records"
echo ""
echo "All timing information is shown above."
echo "The tree structure integrity was verified."

# Create a simple results file
cat > lbtree_benchmark_results.txt << EOF
LBTree Benchmark Results
========================
Date: $(date)
Records processed: $NUM_RECORDS
Operations completed:
- INSERT: $NUM_RECORDS records
- SEARCH: $NUM_RECORDS records  
- DELETE: $DELETE_RECORDS records

Random number generation used your specified macro:
GENERATE_RANDOM_NUMBER_ARRAY with shuffled sequence from 1 to $NUM_RECORDS

All operations completed successfully.
Detailed timing information is available in the console output above.
Tree integrity was verified after all operations.
EOF

echo ""
echo "Results summary saved to lbtree_benchmark_results.txt"

# Clean up temporary files
echo ""
echo "Cleaning up temporary files..."
rm -f generate_benchmark_data
rm -f insert_keys.bin search_keys.bin delete_keys.bin
echo "Cleanup completed."

echo ""
echo "=========================================="
echo "         Benchmark Completed Successfully!"
echo "=========================================="