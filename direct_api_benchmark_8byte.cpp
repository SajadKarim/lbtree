#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

// External function declaration
extern void initUseful(void);

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class DirectAPIBenchmark8Byte {
private:
    const size_t INSERT_RECORDS;
    lbtree* tree;
    
    struct BenchmarkResults {
        double insert_time_ms;
        double lookup_time_ms;
        double delete_time_ms;
        double insert_throughput;
        double lookup_throughput;
        double delete_throughput;
        size_t tree_level;
        bool success;
    } results;

public:
    DirectAPIBenchmark8Byte(size_t insert_records = 10000000) 
        : INSERT_RECORDS(insert_records), tree(nullptr) {
        std::cout << "Direct API Benchmark with 8-byte pointer values - " << INSERT_RECORDS << " records" << std::endl;
        results.success = false;
        results.tree_level = 0;
    }
    
    ~DirectAPIBenchmark8Byte() {
        if (tree) {
            delete tree;
        }
    }
    
    void initializeTree() {
        std::cout << "Initializing LBTree..." << std::endl;
        
        // Initialize useful structures first
        initUseful();
        
        // Initialize worker thread (required)
        worker_thread_num = 1;
        worker_id = 0;
        
        // Initialize memory pool (100MB)
        long long mem_size = 100 * 1024 * 1024; // 100MB
        the_thread_mempools.init(1, mem_size, 4096);
        
        // Initialize NVM pool (1GB for 10M records) - Use Optane PMEM
        const char* nvm_filename = "/mnt/tmpfs/direct_api_benchmark_8byte.nvm";
        long long nvm_size = 1024 * 1024 * 1024; // 1GB
        
        // Clean up any existing NVM file
        std::system("rm -f /mnt/tmpfs/direct_api_benchmark_8byte.nvm");
        
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        // Allocate tree metadata in NVM
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024); // 4KB
        tree = new lbtree(nvm_addr, false);
        
        // Initialize NVM logging
        nvmLogInit(1);
        
        std::cout << "LBTree initialized - Memory: " << (mem_size / (1024*1024)) << "MB, NVM: " << (nvm_size / (1024*1024)) << "MB" << std::endl;
    }
    
    void benchmarkPureInsert() {
        std::cout << "\nINSERT BENCHMARK" << std::endl;
        
        // Generate random keys using your macro
        std::vector<uint64_t> insert_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, INSERT_RECORDS + 1, insert_data);
        
        // Create a minimal key input for bulkload (like debug_insert does)
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        int level = tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "Inserting " << INSERT_RECORDS << " records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Direct API calls to insert all records (store key as pointer value)
        for (size_t i = 0; i < INSERT_RECORDS; i++) {
            key_type key = (key_type)insert_data[i];
            tree->insert(key, (void*)key);  // Store key as 8-byte pointer
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results.insert_time_ms = duration.count() / 1000.0;
        results.insert_throughput = (INSERT_RECORDS * 1000.0) / results.insert_time_ms;
        results.tree_level = tree->level();
        
        std::cout << "INSERT COMPLETED - Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkLookup() {
        std::cout << "\nLOOKUP BENCHMARK" << std::endl;
        
        // Generate lookup keys (same as what we inserted - all 10M records)
        std::vector<uint64_t> lookup_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, INSERT_RECORDS + 1, lookup_data);
        
        size_t lookup_count = INSERT_RECORDS; // Lookup all records
        
        std::cout << "Looking up " << lookup_count << " records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        int found = 0;
        int value_matches = 0;
        for (size_t i = 0; i < lookup_count; i++) {
            key_type key = (key_type)lookup_data[i];
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                found++;
                // Get the stored pointer value
                bleaf* leaf = (bleaf*)result;
                void* stored_value = (void*)leaf->ch(pos).value;
                // Verify that the stored pointer matches the expected key
                if ((key_type)stored_value == key) {
                    value_matches++;
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results.lookup_time_ms = duration.count() / 1000.0;
        results.lookup_throughput = (lookup_count * 1000.0) / results.lookup_time_ms;
        
        std::cout << "LOOKUP COMPLETED - Time: " << std::fixed << std::setprecision(2) << results.lookup_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec, Found: " << found << "/" << lookup_count << ", Value matches: " << value_matches << "/" << found << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\nDELETE BENCHMARK" << std::endl;
        
        // Delete all records that were inserted
        size_t delete_count = INSERT_RECORDS;
        
        std::cout << "Deleting " << delete_count << " records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Delete all keys from 1 to INSERT_RECORDS
        for (size_t i = 1; i <= delete_count; i++) {
            key_type key = (key_type)i;
            tree->del(key);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results.delete_time_ms = duration.count() / 1000.0;
        results.delete_throughput = (delete_count * 1000.0) / results.delete_time_ms;
        
        std::cout << "DELETE COMPLETED - Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec" << std::endl;
    }
    
    void verifyTree() {
        try {
            key_type start, end;
            tree->check(&start, &end);
            std::cout << "Tree verification passed - Key range: " << start << " to " << end << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Tree verification failed: " << e.what() << std::endl;
        }
    }
    
    void printFinalSummary() {
        std::cout << "\nBENCHMARK SUMMARY" << std::endl;
        std::cout << "Records: " << INSERT_RECORDS << std::endl;
        std::cout << "INSERT - Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
        std::cout << "LOOKUP - Time: " << std::fixed << std::setprecision(2) << results.lookup_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec" << std::endl;
        std::cout << "DELETE - Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec" << std::endl;
        std::cout << "Tree Height: " << results.tree_level << " levels" << std::endl;
    }
    
    void runDirectAPIBenchmark() {
        try {
            // Initialize tree and memory pools
            initializeTree();
            
            // Run insert benchmark
            benchmarkPureInsert();
            
            // Verify tree after insert
            verifyTree();
            
            // Run lookup benchmark
            benchmarkLookup();
            
            // Run delete benchmark
            benchmarkDelete();
            
            // Final verification
            verifyTree();
            
            results.success = true;
            
            // Print summary
            printFinalSummary();
            
        } catch (const std::exception& e) {
            std::cerr << "Benchmark failed: " << e.what() << std::endl;
            throw;
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        // Default: 10M insert operations as requested
        size_t insert_records = 10000000;
        
        if (argc > 1) {
            insert_records = std::stoull(argv[1]);
        }
        
        DirectAPIBenchmark8Byte benchmark(insert_records);
        benchmark.runDirectAPIBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}