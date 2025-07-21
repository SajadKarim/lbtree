#!/bin/bash

# Correct LBTree Benchmark Script
# Follows the proper sequence: bulkload first, then performance tests

set -e

echo "=========================================="
echo "         LBTree Correct Benchmark"
echo "=========================================="

# Configuration
NUM_RECORDS=${1:-100000}  # Default 100K for testing
THREAD_NUM=1
MEMPOOL_SIZE=100  # MB
NVMPOOL_SIZE=100  # MB
NVM_FILE="/tmp/lbtree_benchmark.nvm"
FILL_FACTOR=0.7

echo "Configuration:"
echo "  Records: $NUM_RECORDS"
echo "  Threads: $THREAD_NUM"
echo "  Memory Pool: ${MEMPOOL_SIZE} MB"
echo "  NVM Pool: ${NVMPOOL_SIZE} MB"
echo "  Fill Factor: $FILL_FACTOR"

# Clean up
echo ""
echo "Cleaning up previous files..."
rm -f *.bin $NVM_FILE temp_generator*

# Create data generator
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
    
    std::cout << "Generating " << NUM_RECORDS << " records using GENERATE_RANDOM_NUMBER_ARRAY..." << std::endl;
    
    // Generate main dataset for bulkload (70% of records)
    size_t bulkload_records = NUM_RECORDS * 0.7;
    std::vector<uint64_t> bulkload_data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, bulkload_records + 1, bulkload_data);
    std::sort(bulkload_data.begin(), bulkload_data.end()); // bulkload needs sorted data
    
    std::ofstream bulkload_file("bulkload_keys.bin", std::ios::binary);
    bulkload_file.write(reinterpret_cast<const char*>(bulkload_data.data()), bulkload_data.size() * sizeof(uint64_t));
    bulkload_file.close();
    
    // Generate insert data (remaining 30% of records)
    size_t insert_records = NUM_RECORDS - bulkload_records;
    std::vector<uint64_t> insert_data;
    GENERATE_RANDOM_NUMBER_ARRAY(bulkload_records + 1, NUM_RECORDS + 1, insert_data);
    
    std::ofstream insert_file("insert_keys.bin", std::ios::binary);
    insert_file.write(reinterpret_cast<const char*>(insert_data.data()), insert_data.size() * sizeof(uint64_t));
    insert_file.close();
    
    // Generate search data (mix of bulkload and insert data)
    std::vector<uint64_t> search_data = bulkload_data;
    search_data.insert(search_data.end(), insert_data.begin(), insert_data.end());
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(search_data.begin(), search_data.end(), g);
    
    std::ofstream search_file("search_keys.bin", std::ios::binary);
    search_file.write(reinterpret_cast<const char*>(search_data.data()), search_data.size() * sizeof(uint64_t));
    search_file.close();
    
    // Generate delete data (half of all data)
    std::vector<uint64_t> delete_data(search_data.begin(), search_data.begin() + search_data.size() / 2);
    std::shuffle(delete_data.begin(), delete_data.end(), g);
    
    std::ofstream delete_file("delete_keys.bin", std::ios::binary);
    delete_file.write(reinterpret_cast<const char*>(delete_data.data()), delete_data.size() * sizeof(uint64_t));
    delete_file.close();
    
    std::cout << "Data files created:" << std::endl;
    std::cout << "  bulkload_keys.bin: " << bulkload_data.size() << " sorted keys" << std::endl;
    std::cout << "  insert_keys.bin: " << insert_data.size() << " random keys" << std::endl;
    std::cout << "  search_keys.bin: " << search_data.size() << " shuffled keys" << std::endl;
    std::cout << "  delete_keys.bin: " << delete_data.size() << " keys to delete" << std::endl;
    
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
echo "         Running Benchmark"
echo "=========================================="

BULKLOAD_RECORDS=$((NUM_RECORDS * 70 / 100))
INSERT_RECORDS=$((NUM_RECORDS - BULKLOAD_RECORDS))
DELETE_RECORDS=$((NUM_RECORDS / 2))

# Step 1: Bulkload (Tree Preparation)
echo ""
echo "=== BULKLOAD (Tree Preparation) ==="
echo "Bulkloading $BULKLOAD_RECORDS sorted records with fill factor $FILL_FACTOR..."
rm -f $NVM_FILE
echo ""
/usr/bin/time -f "BULKLOAD: %e seconds, %M KB memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE bulkload $BULKLOAD_RECORDS bulkload_keys.bin $FILL_FACTOR
echo ""
echo "Bulkload completed!"

# Step 2: Insert Performance Test
echo ""
echo "=== INSERT PERFORMANCE TEST ==="
echo "Inserting $INSERT_RECORDS additional records..."
echo ""
/usr/bin/time -f "INSERT: %e seconds, %M KB memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE insert $INSERT_RECORDS insert_keys.bin
echo ""
echo "Insert test completed!"

# Step 3: Search Performance Test
echo ""
echo "=== SEARCH PERFORMANCE TEST ==="
echo "Searching $NUM_RECORDS records..."
echo ""
/usr/bin/time -f "SEARCH: %e seconds, %M KB memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE lookup $NUM_RECORDS search_keys.bin
echo ""
echo "Search test completed!"

# Step 4: Delete Performance Test
echo ""
echo "=== DELETE PERFORMANCE TEST ==="
echo "Deleting $DELETE_RECORDS records..."
echo ""
/usr/bin/time -f "DELETE: %e seconds, %M KB memory" \
    ./lbtree thread $THREAD_NUM mempool $MEMPOOL_SIZE nvmpool $NVM_FILE $NVMPOOL_SIZE del $DELETE_RECORDS delete_keys.bin
echo ""
echo "Delete test completed!"

echo ""
echo "=========================================="
echo "            BENCHMARK SUMMARY"
echo "=========================================="
echo "Successfully completed LBTree benchmark:"
echo ""
echo "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro"
echo "Total Records: $NUM_RECORDS uint64_t values"
echo ""
echo "Operations:"
echo "  • BULKLOAD: $BULKLOAD_RECORDS sorted records (tree preparation)"
echo "  • INSERT:   $INSERT_RECORDS random records (performance test)"
echo "  • SEARCH:   $NUM_RECORDS shuffled records (performance test)"
echo "  • DELETE:   $DELETE_RECORDS random records (performance test)"
echo ""
echo "All performance timings are shown above."

# Save results
cat > lbtree_benchmark_results.txt << EOF
LBTree Benchmark Results
========================
Date: $(date)
Total Records: $NUM_RECORDS uint64_t values
Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro

Operations Completed:
- BULKLOAD: $BULKLOAD_RECORDS records (tree preparation)
- INSERT: $INSERT_RECORDS records (performance test)
- SEARCH: $NUM_RECORDS records (performance test)
- DELETE: $DELETE_RECORDS records (performance test)

Configuration:
- Threads: $THREAD_NUM
- Memory Pool: ${MEMPOOL_SIZE} MB
- NVM Pool: ${NVMPOOL_SIZE} MB
- Fill Factor: $FILL_FACTOR

All operations completed successfully.
Performance timings available in console output.
EOF

echo ""
echo "Results saved to lbtree_benchmark_results.txt"

# Cleanup
echo ""
echo "Cleaning up..."
rm -f temp_generator temp_generator.cpp
rm -f *.bin
echo "Cleanup completed."

echo ""
echo "Benchmark completed successfully!"