#!/bin/bash

# Simple LBTree Benchmark Script
# This script benchmarks insert, search, and delete operations on 10M uint64_t records

set -e

echo "=========================================="
echo "         LBTree Benchmark Script"
echo "=========================================="

# Configuration
NUM_RECORDS=10000000
THREAD_NUM=1
MEMPOOL_SIZE=2048  # MB
NVMPOOL_SIZE=2048  # MB
NVM_FILE="/tmp/lbtree_benchmark.nvm"

# Clean up previous files
echo "Cleaning up previous benchmark files..."
rm -f insert_keys.bin search_keys.bin delete_keys.bin
rm -f $NVM_FILE
rm -f lbtree_benchmark_results.txt

# Step 1: Build the data generator
echo "Building data generator..."
g++ -O3 -std=c++11 -o generate_benchmark_data generate_benchmark_data.cpp

# Step 2: Generate benchmark data
echo "Generating benchmark data..."
./generate_benchmark_data

# Step 3: Build lbtree
echo "Building lbtree..."
make clean
make lbtree

# Step 4: Run benchmarks
echo ""
echo "=========================================="
echo "         Starting Benchmarks"
echo "=========================================="

# Initialize and run insert benchmark
echo ""
echo "=== INSERT BENCHMARK ==="
echo "Inserting $NUM_RECORDS records..."

echo "Running: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $NUM_RECORDS insert_keys.bin"
time ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $NUM_RECORDS insert_keys.bin

# Run search benchmark
echo ""
echo "=== SEARCH BENCHMARK ==="
echo "Searching $NUM_RECORDS records..."

echo "Running: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin"
time ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin

# Run delete benchmark
DELETE_RECORDS=$((NUM_RECORDS / 2))
echo ""
echo "=== DELETE BENCHMARK ==="
echo "Deleting $DELETE_RECORDS records..."

echo "Running: ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin"
time ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin

echo ""
echo "=========================================="
echo "         Benchmark Completed"
echo "=========================================="
echo "All operations completed successfully!"
echo "Performance times are shown above using the 'time' command."

# Clean up
echo ""
echo "Cleaning up temporary files..."
rm -f generate_benchmark_data
rm -f insert_keys.bin search_keys.bin delete_keys.bin
echo "Cleanup completed."