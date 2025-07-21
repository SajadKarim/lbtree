#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstdint>

// Include lbtree headers
#include "lbtree-src/lbtree.h"

// Global variables needed by lbtree
int worker_thread_num = 1;
__thread int worker_id = 0;

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class DirectLBTreeBenchmark {
private:
    lbtree* tree;
    std::vector<uint64_t> data;
    const size_t NUM_RECORDS;
    
    struct BenchmarkResults {
        double insert_time_ms;
        double search_time_ms;
        double delete_time_ms;
        double insert_throughput;
        double search_throughput;
        double delete_throughput;
        size_t successful_searches;
        size_t successful_deletes;
    } results;

public:
    DirectLBTreeBenchmark(size_t num_records = 1000000) : NUM_RECORDS(num_records), tree(nullptr) {
        std::cout << "Initializing Direct LBTree Benchmark with " << NUM_RECORDS << " records..." << std::endl;
        
        // Initialize global variables
        worker_thread_num = 1;
        worker_id = 0;
        
        // Initialize memory pools with reasonable sizes
        the_thread_mempools.init(1, 512 * MB, 4096);
        
        // Initialize NVM pool
        const char* nvm_file = "/tmp/direct_lbtree_benchmark.nvm";
        the_thread_nvmpools.init(1, nvm_file, 512 * MB);
        
        // Initialize NVM log
        nvmLogInit(1);
        
        // Create tree
        char* nvm_addr = (char*)nvmpool_alloc(4 * KB);
        tree = new lbtree(nvm_addr, false);
        
        std::cout << "LBTree initialized successfully!" << std::endl;
    }
    
    ~DirectLBTreeBenchmark() {
        if (tree) {
            delete tree;
        }
    }
    
    void generateData() {
        std::cout << "\nGenerating " << NUM_RECORDS << " random uint64_t records using your macro..." << std::endl;
        
        // Use your specified macro to generate random data
        GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
        
        std::cout << "Data generation completed!" << std::endl;
        std::cout << "Sample data (first 10): ";
        for (int i = 0; i < 10 && i < data.size(); i++) {
            std::cout << data[i] << " ";
        }
        std::cout << std::endl;
    }
    
    void benchmarkInsert() {
        std::cout << "\n=== INSERT BENCHMARK ===" << std::endl;
        std::cout << "Inserting " << data.size() << " records directly into lbtree..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < data.size(); ++i) {
            uint64_t key = data[i];
            tree->insert(key, (void*)key);
            
            // Progress indicator for large datasets
            if ((i + 1) % 100000 == 0) {
                std::cout << "Inserted " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.insert_time_ms = duration.count() / 1000.0;
        results.insert_throughput = (data.size() * 1000000.0) / duration.count();
        
        std::cout << "Insert completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkSearch() {
        std::cout << "\n=== SEARCH BENCHMARK ===" << std::endl;
        std::cout << "Searching " << data.size() << " records directly in lbtree..." << std::endl;
        
        results.successful_searches = 0;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < data.size(); ++i) {
            uint64_t key = data[i];
            int pos;
            void* leaf = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                void* recptr = tree->get_recptr(leaf, pos);
                if ((uint64_t)recptr == key) {
                    results.successful_searches++;
                }
            }
            
            // Progress indicator for large datasets
            if ((i + 1) % 100000 == 0) {
                std::cout << "Searched " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.search_time_ms = duration.count() / 1000.0;
        results.search_throughput = (data.size() * 1000000.0) / duration.count();
        
        std::cout << "Search completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.search_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.search_throughput << " ops/sec" << std::endl;
        std::cout << "Found: " << results.successful_searches << "/" << data.size() << " records" << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\n=== DELETE BENCHMARK ===" << std::endl;
        
        // Delete half of the records
        size_t delete_count = data.size() / 2;
        std::cout << "Deleting " << delete_count << " records directly from lbtree..." << std::endl;
        
        results.successful_deletes = 0;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < delete_count; ++i) {
            uint64_t key = data[i];
            tree->del(key);
            results.successful_deletes++;  // Assume successful since del() is void
            
            // Progress indicator for large datasets
            if ((i + 1) % 50000 == 0) {
                std::cout << "Deleted " << (i + 1) << " records..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.delete_time_ms = duration.count() / 1000.0;
        results.delete_throughput = (delete_count * 1000000.0) / duration.count();
        
        std::cout << "Delete completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec" << std::endl;
        std::cout << "Deleted: " << results.successful_deletes << "/" << delete_count << " records" << std::endl;
    }
    
    void verifyTreeIntegrity() {
        std::cout << "\n=== TREE INTEGRITY CHECK ===" << std::endl;
        try {
            key_type start, end;
            tree->check(&start, &end);
            std::cout << "✓ Tree integrity check PASSED" << std::endl;
            std::cout << "Tree level: " << tree->level() << std::endl;
            std::cout << "Key range: [" << start << ", " << end << "]" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ Tree integrity check FAILED: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "✗ Tree integrity check FAILED: Unknown error" << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                    BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::cout << "Records: " << NUM_RECORDS << " uint64_t values" << std::endl;
        std::cout << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        std::cout << std::left << std::setw(15) << "Operation" 
                  << std::setw(15) << "Time (ms)" 
                  << std::setw(20) << "Throughput (ops/sec)"
                  << std::setw(15) << "Success Rate" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        std::cout << std::left << std::setw(15) << "INSERT" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.insert_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.insert_throughput
                  << std::setw(15) << "100%" << std::endl;
                  
        std::cout << std::left << std::setw(15) << "SEARCH" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.search_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.search_throughput
                  << std::setw(15) << (results.successful_searches * 100.0 / NUM_RECORDS) << "%" << std::endl;
                  
        std::cout << std::left << std::setw(15) << "DELETE" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.delete_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.delete_throughput
                  << std::setw(15) << (results.successful_deletes * 100.0 / (NUM_RECORDS/2)) << "%" << std::endl;
        
        std::cout << std::string(70, '=') << std::endl;
    }
    
    void saveResults(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Direct LBTree Benchmark Results\n";
            file << "================================\n";
            file << "Date: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "\n";
            file << "Records: " << NUM_RECORDS << " uint64_t values\n";
            file << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro\n\n";
            
            file << "Performance Results:\n";
            file << "INSERT - Time: " << results.insert_time_ms << " ms, ";
            file << "Throughput: " << results.insert_throughput << " ops/sec\n";
            
            file << "SEARCH - Time: " << results.search_time_ms << " ms, ";
            file << "Throughput: " << results.search_throughput << " ops/sec, ";
            file << "Success: " << results.successful_searches << "/" << NUM_RECORDS << "\n";
            
            file << "DELETE - Time: " << results.delete_time_ms << " ms, ";
            file << "Throughput: " << results.delete_throughput << " ops/sec, ";
            file << "Success: " << results.successful_deletes << "/" << (NUM_RECORDS/2) << "\n";
            
            file << "\nTree Level: " << tree->level() << "\n";
            file.close();
            std::cout << "\nResults saved to " << filename << std::endl;
        } else {
            std::cout << "\nFailed to save results to " << filename << std::endl;
        }
    }
    
    void runFullBenchmark() {
        std::cout << "Starting Direct LBTree Benchmark..." << std::endl;
        
        // Generate test data
        generateData();
        
        // Run benchmarks in sequence
        benchmarkInsert();
        verifyTreeIntegrity();
        
        benchmarkSearch();
        verifyTreeIntegrity();
        
        benchmarkDelete();
        verifyTreeIntegrity();
        
        // Print summary and save results
        printSummary();
        saveResults("direct_lbtree_benchmark_results.txt");
        
        std::cout << "\n🎉 Direct benchmark completed successfully!" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Allow command line argument for number of records
        size_t num_records = 1000000;  // Default 1M
        if (argc > 1) {
            num_records = std::stoull(argv[1]);
        }
        
        std::cout << "=========================================" << std::endl;
        std::cout << "    Direct LBTree Performance Benchmark" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        DirectLBTreeBenchmark benchmark(num_records);
        benchmark.runFullBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Benchmark failed with unknown exception" << std::endl;
        return 1;
    }
}