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

class DirectAPIBenchmark {
private:
    const size_t INSERT_RECORDS;
    lbtree* tree;
    std::vector<uint64_t> operation_data;  // Single dataset for all operations
    std::vector<char*> value_pointers;     // Track our individually allocated value pointers
    
    // Helper function to create 16-byte value from key
    void createValue(key_type key, char* value) {
        // Clear the array
        memset(value, 0, 16);
        // Store key in first 8 bytes
        memcpy(value, &key, sizeof(key_type));
        // Fill remaining 8 bytes with pattern based on key
        for (int i = 8; i < 16; i++) {
            value[i] = (char)((key >> ((i-8) * 8)) & 0xFF);
        }
    }
    
    // Helper function to verify value matches expected key
    bool verifyValue(key_type key, const char* value) {
        key_type stored_key;
        memcpy(&stored_key, value, sizeof(key_type));
        return stored_key == key;
    }
    
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
    DirectAPIBenchmark(size_t insert_records = 10000000) 
        : INSERT_RECORDS(insert_records), tree(nullptr) {
        std::cout << "Direct API Benchmark with 16-byte values - " << INSERT_RECORDS << " records" << std::endl;
        results.success = false;
        results.tree_level = 0;
        
        // Generate single dataset for all operations
        GENERATE_RANDOM_NUMBER_ARRAY(1, INSERT_RECORDS + 1, operation_data);
        std::cout << "Generated random dataset for " << INSERT_RECORDS << " operations" << std::endl;
        
        // Initialize value pointers vector (will be populated during insert)
        value_pointers.resize(INSERT_RECORDS, nullptr);
        std::cout << "Prepared to track " << INSERT_RECORDS << " individual value pointers" << std::endl;
    }
    
    ~DirectAPIBenchmark() {
        if (tree) {
            delete tree;
        }
        // Clean up individually allocated values
        for (size_t i = 0; i < value_pointers.size(); i++) {
            if (value_pointers[i] != nullptr) {
                delete[] value_pointers[i];
            }
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
        const char* nvm_filename = "/mnt/tmpfs/direct_api_benchmark_16byte.nvm";
        long long nvm_size = 1024 * 1024 * 1024; // 1GB
        
        // Clean up any existing NVM file
        std::system("rm -f /mnt/tmpfs/direct_api_benchmark_16byte.nvm");
        
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
        
        // Create a minimal key input for bulkload (like debug_insert does)
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        int level = tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "Inserting " << INSERT_RECORDS << " records using shared dataset..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();

        // CRITICAL: Flush actual 16-byte values to NVM file (part of insert timing)
        //std::cout << "Flushing " << INSERT_RECORDS << " 16-byte values to NVM..." << std::endl;
        std::string nvm_pointers_file = "/mnt/tmpfs/value_pointers_" + std::to_string(INSERT_RECORDS) + ".dat";
        std::ofstream nvm_file(nvm_pointers_file, std::ios::binary);

        // Direct API calls to insert all records using shared dataset
        for (size_t i = 0; i < INSERT_RECORDS; i++) {
            key_type key = (key_type)operation_data[i];
            
            // Allocate individual 16-byte value
            char* individual_value = new char[16];
            createValue(key, individual_value);
            
            // Track our individually allocated pointer
            value_pointers[i] = individual_value;
            
            // Insert into LBTree
            tree->insert(key, (void*)individual_value);

            // Write each 16-byte value to NVM file
            nvm_file.write(value_pointers[i], 16);
        }
        
        // Close and sync the NVM file after all writes
        nvm_file.flush();
        nvm_file.close();
        
        // Force filesystem sync to ensure data reaches NVM
        //std::system(("sync " + nvm_pointers_file).c_str());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results.insert_time_ms = duration.count() / 1000.0;
        results.insert_throughput = (INSERT_RECORDS * 1000.0) / results.insert_time_ms;
        results.tree_level = tree->level();
        
        std::cout << "INSERT COMPLETED - Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkLookup() {
        std::cout << "\nLOOKUP BENCHMARK" << std::endl;
        
        size_t lookup_count = INSERT_RECORDS; // Lookup all records
        
        std::cout << "Looking up " << lookup_count << " records using shared dataset..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        int found = 0;
        int pointer_matches = 0;
        int content_matches = 0;
        for (size_t i = 0; i < lookup_count; i++) {
            key_type key = (key_type)operation_data[i];
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                found++;
                
                // Get pointer from LBTree
                bleaf* leaf = (bleaf*)result;
                void* lbtree_ptr = (void*)leaf->ch(pos).value;
                
                // Compare with our tracked pointer
                char* expected_ptr = value_pointers[i];
                if (lbtree_ptr == (void*)expected_ptr) {
                    pointer_matches++;
                }
                
                // CRITICAL: Actually access and compare the 16-byte content
                // This forces CPU to load the actual values from memory
                // char* actual_value = (char*)lbtree_ptr;
                // if (memcmp(actual_value, expected_ptr, 16) == 0) {
                //     content_matches++;
                // }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results.lookup_time_ms = duration.count() / 1000.0;
        results.lookup_throughput = (lookup_count * 1000.0) / results.lookup_time_ms;
        
        std::cout << "LOOKUP COMPLETED - Time: " << std::fixed << std::setprecision(2) << results.lookup_time_ms << " ms, Throughput: " << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec, Found: " << found << "/" << lookup_count << ", Pointer matches: " << pointer_matches << "/" << found << ", Content matches: " << content_matches << "/" << found << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\nDELETE BENCHMARK" << std::endl;
        
        size_t delete_count = INSERT_RECORDS;
        
        std::cout << "Deleting " << delete_count << " records using shared dataset..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Delete using same random order as insert/lookup
        for (size_t i = 0; i < delete_count; i++) {
            key_type key = (key_type)operation_data[i];
            tree->del(key);
            delete [] value_pointers[i]; // Clean up individual value
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
        
        DirectAPIBenchmark benchmark(insert_records);
        benchmark.runDirectAPIBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}