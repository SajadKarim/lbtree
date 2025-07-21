#!/bin/bash

# LBTree Benchmark Script
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

# Measure insert time
INSERT_START=$(date +%s%N)
./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $NUM_RECORDS insert_keys.bin
INSERT_END=$(date +%s%N)

INSERT_TIME_NS=$((INSERT_END - INSERT_START))
INSERT_TIME_MS=$((INSERT_TIME_NS / 1000000))
INSERT_THROUGHPUT=$(echo "scale=0; $NUM_RECORDS * 1000000000 / $INSERT_TIME_NS" | bc -l)

echo "Insert Time: ${INSERT_TIME_MS} ms"
echo "Insert Throughput: ${INSERT_THROUGHPUT} ops/sec"

# Run search benchmark
echo ""
echo "=== SEARCH BENCHMARK ==="
echo "Searching $NUM_RECORDS records..."

SEARCH_START=$(date +%s%N)
./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin
SEARCH_END=$(date +%s%N)

SEARCH_TIME_NS=$((SEARCH_END - SEARCH_START))
SEARCH_TIME_MS=$((SEARCH_TIME_NS / 1000000))
SEARCH_THROUGHPUT=$(echo "scale=0; $NUM_RECORDS * 1000000000 / $SEARCH_TIME_NS" | bc -l)

echo "Search Time: ${SEARCH_TIME_MS} ms"
echo "Search Throughput: ${SEARCH_THROUGHPUT} ops/sec"

# Run delete benchmark
DELETE_RECORDS=$((NUM_RECORDS / 2))
echo ""
echo "=== DELETE BENCHMARK ==="
echo "Deleting $DELETE_RECORDS records..."

DELETE_START=$(date +%s%N)
./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin
DELETE_END=$(date +%s%N)

DELETE_TIME_NS=$((DELETE_END - DELETE_START))
DELETE_TIME_MS=$((DELETE_TIME_NS / 1000000))
DELETE_THROUGHPUT=$(echo "scale=0; $DELETE_RECORDS * 1000000000 / $DELETE_TIME_NS" | bc -l)

echo "Delete Time: ${DELETE_TIME_MS} ms"
echo "Delete Throughput: ${DELETE_THROUGHPUT} ops/sec"

# Generate summary
echo ""
echo "=========================================="
echo "            BENCHMARK SUMMARY"
echo "=========================================="
printf "%-15s %-15s %-20s\n" "Operation" "Time (ms)" "Throughput (ops/sec)"
echo "--------------------------------------------------"
printf "%-15s %-15s %-20s\n" "INSERT" "$INSERT_TIME_MS" "$INSERT_THROUGHPUT"
printf "%-15s %-15s %-20s\n" "SEARCH" "$SEARCH_TIME_MS" "$SEARCH_THROUGHPUT"
printf "%-15s %-15s %-20s\n" "DELETE" "$DELETE_TIME_MS" "$DELETE_THROUGHPUT"
echo "=========================================="

# Save results to file
cat > lbtree_benchmark_results.txt << EOF
LBTree Benchmark Results
========================
Records: $NUM_RECORDS
Insert Time (ms): $INSERT_TIME_MS
Insert Throughput (ops/sec): $INSERT_THROUGHPUT
Search Time (ms): $SEARCH_TIME_MS
Search Throughput (ops/sec): $SEARCH_THROUGHPUT
Delete Records: $DELETE_RECORDS
Delete Time (ms): $DELETE_TIME_MS
Delete Throughput (ops/sec): $DELETE_THROUGHPUT
EOF

echo ""
echo "Results saved to lbtree_benchmark_results.txt"
echo "Benchmark completed successfully!"

# Clean up
echo ""
echo "Cleaning up temporary files..."
rm -f generate_benchmark_data
rm -f insert_keys.bin search_keys.bin delete_keys.bin
echo "Cleanup completed."