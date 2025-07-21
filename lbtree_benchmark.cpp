#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <cassert>

// Include lbtree headers
#include "lbtree-src/lbtree.h"

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class LBTreeBenchmark {
private:
    lbtree* tree;
    std::vector<uint64_t> data;
    std::vector<uint64_t> search_keys;
    std::vector<uint64_t> delete_keys;
    const size_t NUM_RECORDS = 10000000; // 10M records
    
    // Performance results
    struct BenchmarkResults {
        double insert_time_ms;
        double search_time_ms;
        double delete_time_ms;
        double insert_throughput_ops_per_sec;
        double search_throughput_ops_per_sec;
        double delete_throughput_ops_per_sec;
    } results;

public:
    LBTreeBenchmark() : tree(nullptr) {
        // Initialize memory and NVM pools
        worker_thread_num = 1;
        worker_id = 0;
        
        // Initialize memory pool (1GB)
        the_thread_mempools.init(1, 1024LL * MB, 4096);
        
        // Initialize NVM pool (1GB to avoid overflow)
        const char* nvm_file = "/tmp/lbtree_benchmark.nvm";
        the_thread_nvmpools.init(1, nvm_file, 1024LL * MB);
        
        // Allocate tree
        char* nvm_addr = (char*)nvmpool_alloc(4 * KB);
        tree = new lbtree(nvm_addr, false);
        
        // Initialize NVM log
        nvmLogInit(1);
        
        std::cout << "LBTree Benchmark initialized with " << NUM_RECORDS << " records" << std::endl;
    }
    
    ~LBTreeBenchmark() {
        if (tree) {
            delete tree;
        }
    }
    
    void generateData() {
        std::cout << "Generating " << NUM_RECORDS << " random uint64_t records..." << std::endl;
        
        // Generate shuffled sequence from 1 to NUM_RECORDS
        GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
        
        // Create search keys (same as insert keys for successful searches)
        search_keys = data;
        
        // Create delete keys (first half of the data for partial deletion)
        delete_keys.resize(NUM_RECORDS / 2);
        std::copy(data.begin(), data.begin() + NUM_RECORDS / 2, delete_keys.begin());
        
        // Shuffle delete keys to avoid sequential deletion
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(delete_keys.begin(), delete_keys.end(), g);
        
        std::cout << "Data generation completed." << std::endl;
        std::cout << "Insert keys: " << data.size() << std::endl;
        std::cout << "Search keys: " << search_keys.size() << std::endl;
        std::cout << "Delete keys: " << delete_keys.size() << std::endl;
    }
    
    void benchmarkInsert() {
        std::cout << "\n=== INSERT BENCHMARK ===" << std::endl;
        std::cout << "Inserting " << data.size() << " records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < data.size(); ++i) {
            uint64_t key = data[i];
            tree->insert(key, (void*)key);
            
            // Progress indicator
            if ((i + 1) % 1000000 == 0) {
                std::cout << "Inserted " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.insert_time_ms = duration.count() / 1000.0;
        results.insert_throughput_ops_per_sec = (data.size() * 1000000.0) / duration.count();
        
        std::cout << "Insert completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput_ops_per_sec << " ops/sec" << std::endl;
    }
    
    void benchmarkSearch() {
        std::cout << "\n=== SEARCH BENCHMARK ===" << std::endl;
        std::cout << "Searching " << search_keys.size() << " records..." << std::endl;
        
        size_t found_count = 0;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < search_keys.size(); ++i) {
            uint64_t key = search_keys[i];
            int pos;
            void* leaf = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                void* recptr = tree->get_recptr(leaf, pos);
                if ((uint64_t)recptr == key) {
                    found_count++;
                }
            }
            
            // Progress indicator
            if ((i + 1) % 1000000 == 0) {
                std::cout << "Searched " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.search_time_ms = duration.count() / 1000.0;
        results.search_throughput_ops_per_sec = (search_keys.size() * 1000000.0) / duration.count();
        
        std::cout << "Search completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.search_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.search_throughput_ops_per_sec << " ops/sec" << std::endl;
        std::cout << "Found: " << found_count << "/" << search_keys.size() << " records" << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\n=== DELETE BENCHMARK ===" << std::endl;
        std::cout << "Deleting " << delete_keys.size() << " records..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < delete_keys.size(); ++i) {
            uint64_t key = delete_keys[i];
            tree->del(key);
            
            // Progress indicator
            if ((i + 1) % 500000 == 0) {
                std::cout << "Deleted " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.delete_time_ms = duration.count() / 1000.0;
        results.delete_throughput_ops_per_sec = (delete_keys.size() * 1000000.0) / duration.count();
        
        std::cout << "Delete completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput_ops_per_sec << " ops/sec" << std::endl;
    }
    
    void verifyTreeIntegrity() {
        std::cout << "\n=== TREE INTEGRITY CHECK ===" << std::endl;
        try {
            key_type start, end;
            tree->check(&start, &end);
            std::cout << "Tree integrity check PASSED" << std::endl;
            std::cout << "Tree level: " << tree->level() << std::endl;
            std::cout << "Key range: [" << start << ", " << end << "]" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Tree integrity check FAILED: " << e.what() << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "                BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << std::left << std::setw(20) << "Operation" 
                  << std::setw(15) << "Time (ms)" 
                  << std::setw(20) << "Throughput (ops/sec)" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        std::cout << std::left << std::setw(20) << "INSERT" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.insert_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.insert_throughput_ops_per_sec << std::endl;
                  
        std::cout << std::left << std::setw(20) << "SEARCH" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.search_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.search_throughput_ops_per_sec << std::endl;
                  
        std::cout << std::left << std::setw(20) << "DELETE" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.delete_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.delete_throughput_ops_per_sec << std::endl;
        
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void saveResultsToFile(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "LBTree Benchmark Results\n";
            file << "========================\n";
            file << "Records: " << NUM_RECORDS << "\n";
            file << "Insert Time (ms): " << results.insert_time_ms << "\n";
            file << "Insert Throughput (ops/sec): " << results.insert_throughput_ops_per_sec << "\n";
            file << "Search Time (ms): " << results.search_time_ms << "\n";
            file << "Search Throughput (ops/sec): " << results.search_throughput_ops_per_sec << "\n";
            file << "Delete Time (ms): " << results.delete_time_ms << "\n";
            file << "Delete Throughput (ops/sec): " << results.delete_throughput_ops_per_sec << "\n";
            file << "Tree Level: " << tree->level() << "\n";
            file.close();
            std::cout << "Results saved to " << filename << std::endl;
        } else {
            std::cout << "Failed to save results to " << filename << std::endl;
        }
    }
    
    void runBenchmark() {
        std::cout << "Starting LBTree Benchmark..." << std::endl;
        
        // Generate test data
        generateData();
        
        // Run benchmarks
        benchmarkInsert();
        verifyTreeIntegrity();
        
        benchmarkSearch();
        
        benchmarkDelete();
        verifyTreeIntegrity();
        
        // Print summary
        printSummary();
        
        // Save results
        saveResultsToFile("lbtree_benchmark_results.txt");
        
        std::cout << "\nBenchmark completed successfully!" << std::endl;
    }
};

int main() {
    try {
        LBTreeBenchmark benchmark;
        benchmark.runBenchmark();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Benchmark failed with unknown exception" << std::endl;
        return 1;
    }
}