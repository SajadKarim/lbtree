#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

class RangeQueryTest {
private:
    lbtree* tree;
    char* value_storage_16byte;
    const size_t NUM_RECORDS = 1000000;  // 1M records
    const size_t RANGE_SIZE = 10000;     // 10K range queries
    
public:
    RangeQueryTest() : tree(nullptr), value_storage_16byte(nullptr) {}
    
    ~RangeQueryTest() {
        if (tree) delete tree;
        if (value_storage_16byte) delete[] value_storage_16byte;
    }
    
    void initialize() {
        std::cout << "========================================" << std::endl;
        std::cout << "RANGE QUERY PERFORMANCE TEST" << std::endl;
        std::cout << "========================================" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 500 * 1024 * 1024;  // 500MB
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/range_query_test.nvm";
        long long nvm_size = 2LL * 1024 * 1024 * 1024;  // 2GB
        std::system("rm -f /mnt/tmpfs/range_query_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        std::cout << "✓ LBTree initialized for " << NUM_RECORDS << " records" << std::endl;
    }
    
    void test8ByteRangeQuery() {
        std::cout << "\n=== 8-BYTE VALUE RANGE QUERY TEST ===" << std::endl;
        std::cout << "Values stored directly (no indirection)" << std::endl;
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Insert 8-byte values (stored directly as pointers)
        std::cout << "Inserting " << NUM_RECORDS << " records with 8-byte values..." << std::endl;
        auto start_insert = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = i + 1;
            uint64_t value = 0xDEADBEEF00000000ULL + i;  // Unique 8-byte value
            tree->insert(key, (void*)value);
        }
        
        auto end_insert = std::chrono::high_resolution_clock::now();
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_insert - start_insert);
        std::cout << "Insert time: " << insert_time.count() << " ms" << std::endl;
        
        // Range query test
        std::cout << "Performing range queries (size=" << RANGE_SIZE << ")..." << std::endl;
        auto start_range = std::chrono::high_resolution_clock::now();
        
        size_t total_found = 0;
        size_t num_ranges = 100;  // 100 different ranges
        
        for (size_t r = 0; r < num_ranges; r++) {
            key_type start_key = (r * 1000) + 1;
            key_type end_key = start_key + RANGE_SIZE - 1;
            
            // Simulate range query by individual lookups
            for (key_type key = start_key; key <= end_key && key <= NUM_RECORDS; key++) {
                int pos;
                void* result = tree->lookup(key, &pos);
                if (pos >= 0) {
                    bleaf* leaf = (bleaf*)result;
                    uint64_t value = (uint64_t)leaf->ch(pos).value;  // Direct access!
                    total_found++;
                    
                    // Simulate some processing
                    volatile uint64_t processed = value + 1;
                    (void)processed;
                }
            }
        }
        
        auto end_range = std::chrono::high_resolution_clock::now();
        auto range_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_range - start_range);
        
        std::cout << "Range query time: " << range_time.count() << " ms" << std::endl;
        std::cout << "Total values processed: " << total_found << std::endl;
        std::cout << "Throughput: " << (total_found * 1000.0 / range_time.count()) << " ops/sec" << std::endl;
        
        // Clear tree for next test
        delete tree;
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr, false);
    }
    
    void test16ByteRangeQuery() {
        std::cout << "\n=== 16-BYTE VALUE RANGE QUERY TEST ===" << std::endl;
        std::cout << "Values stored as pointers (with indirection)" << std::endl;
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Allocate storage for 16-byte values
        value_storage_16byte = new char[NUM_RECORDS * 16];
        std::cout << "Allocated " << (NUM_RECORDS * 16 / 1024 / 1024) << " MB for 16-byte values" << std::endl;
        
        // Insert 16-byte values (stored as pointers to DRAM)
        std::cout << "Inserting " << NUM_RECORDS << " records with 16-byte values..." << std::endl;
        auto start_insert = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = i + 1;
            char* value_ptr = &value_storage_16byte[i * 16];
            
            // Fill with recognizable pattern
            snprintf(value_ptr, 16, "VAL_%010zu", i);
            
            tree->insert(key, (void*)value_ptr);  // Store pointer!
        }
        
        auto end_insert = std::chrono::high_resolution_clock::now();
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_insert - start_insert);
        std::cout << "Insert time: " << insert_time.count() << " ms" << std::endl;
        
        // Range query test
        std::cout << "Performing range queries (size=" << RANGE_SIZE << ")..." << std::endl;
        auto start_range = std::chrono::high_resolution_clock::now();
        
        size_t total_found = 0;
        size_t num_ranges = 100;  // 100 different ranges
        
        for (size_t r = 0; r < num_ranges; r++) {
            key_type start_key = (r * 1000) + 1;
            key_type end_key = start_key + RANGE_SIZE - 1;
            
            // Simulate range query by individual lookups
            for (key_type key = start_key; key <= end_key && key <= NUM_RECORDS; key++) {
                int pos;
                void* result = tree->lookup(key, &pos);
                if (pos >= 0) {
                    bleaf* leaf = (bleaf*)result;
                    void* value_ptr = (void*)leaf->ch(pos).value;  // Get pointer
                    char* value_data = (char*)value_ptr;           // +1 indirection!
                    total_found++;
                    
                    // Simulate some processing (access the actual data)
                    volatile char first_char = value_data[0];
                    volatile char last_char = value_data[14];
                    (void)first_char; (void)last_char;
                }
            }
        }
        
        auto end_range = std::chrono::high_resolution_clock::now();
        auto range_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_range - start_range);
        
        std::cout << "Range query time: " << range_time.count() << " ms" << std::endl;
        std::cout << "Total values processed: " << total_found << std::endl;
        std::cout << "Throughput: " << (total_found * 1000.0 / range_time.count()) << " ops/sec" << std::endl;
    }
    
    void compareResults() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "RANGE QUERY PERFORMANCE COMPARISON" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << "EXPECTED RESULTS:" << std::endl;
        std::cout << "• 8-byte values: FASTER (direct access, no indirection)" << std::endl;
        std::cout << "• 16-byte values: SLOWER (pointer + data access)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "WHY 16-BYTE VALUES ARE SLOWER:" << std::endl;
        std::cout << "1. Extra memory access per value (pointer indirection)" << std::endl;
        std::cout << "2. Poor cache locality (values scattered in DRAM)" << std::endl;
        std::cout << "3. Memory bandwidth bottleneck (2x memory accesses)" << std::endl;
        std::cout << "4. TLB pressure (accessing scattered memory pages)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "IMPLICATIONS:" << std::endl;
        std::cout << "• Range queries expose the indirection overhead" << std::endl;
        std::cout << "• LBTree's pointer-based approach hurts range performance" << std::endl;
        std::cout << "• True persistent storage would be even slower (NVM access)" << std::endl;
        std::cout << "• 8-byte values are the sweet spot for LBTree" << std::endl;
    }
    
    void run() {
        initialize();
        test8ByteRangeQuery();
        test16ByteRangeQuery();
        compareResults();
    }
};

int main() {
    try {
        RangeQueryTest test;
        test.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}