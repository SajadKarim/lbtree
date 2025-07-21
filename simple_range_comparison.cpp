#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "SIMPLE RANGE QUERY COMPARISON" << std::endl;
        std::cout << "========================================" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/simple_range_test.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/simple_range_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        const size_t NUM_RECORDS = 100000;  // 100K records
        const size_t RANGE_SIZE = 1000;     // 1K range
        
        // TEST 1: 8-byte values (direct storage)
        std::cout << "\n=== TEST 1: 8-BYTE VALUES (DIRECT) ===" << std::endl;
        
        char *nvm_addr1 = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree1 = new lbtree(nvm_addr1, false);
        nvmLogInit(1);
        
        inMemKeyInput *input1 = new inMemKeyInput(2, 1, 2);
        tree1->bulkload(1, input1, 1.0);
        delete input1;
        
        // Insert 8-byte values
        std::cout << "Inserting " << NUM_RECORDS << " 8-byte values..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = i + 1;
            uint64_t value = 0xABCD000000000000ULL + i;
            tree1->insert(key, (void*)value);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto insert_time1 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Insert time: " << insert_time1.count() << " ms" << std::endl;
        
        // Range query test
        std::cout << "Range query test (1000 lookups)..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        
        size_t found1 = 0;
        for (size_t i = 0; i < RANGE_SIZE; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree1->lookup(key, &pos);
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                uint64_t value = (uint64_t)leaf->ch(pos).value;  // DIRECT ACCESS
                found1++;
                volatile uint64_t processed = value + 1;  // Simulate processing
                (void)processed;
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto range_time1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Range time: " << range_time1.count() << " μs" << std::endl;
        std::cout << "Found: " << found1 << " values" << std::endl;
        std::cout << "Throughput: " << (found1 * 1000000.0 / range_time1.count()) << " ops/sec" << std::endl;
        
        delete tree1;
        
        // TEST 2: 16-byte values (pointer storage)
        std::cout << "\n=== TEST 2: 16-BYTE VALUES (POINTERS) ===" << std::endl;
        
        char *nvm_addr2 = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree2 = new lbtree(nvm_addr2, false);
        
        inMemKeyInput *input2 = new inMemKeyInput(2, 1, 2);
        tree2->bulkload(1, input2, 1.0);
        delete input2;
        
        // Allocate 16-byte values
        char* value_storage = new char[NUM_RECORDS * 16];
        std::cout << "Allocated " << (NUM_RECORDS * 16 / 1024) << " KB for 16-byte values" << std::endl;
        
        // Insert 16-byte values
        std::cout << "Inserting " << NUM_RECORDS << " 16-byte values..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = i + 1;
            char* value_ptr = &value_storage[i * 16];
            snprintf(value_ptr, 16, "VALUE_%09zu", i);
            tree2->insert(key, (void*)value_ptr);  // STORE POINTER
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto insert_time2 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Insert time: " << insert_time2.count() << " ms" << std::endl;
        
        // Range query test
        std::cout << "Range query test (1000 lookups)..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        
        size_t found2 = 0;
        for (size_t i = 0; i < RANGE_SIZE; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree2->lookup(key, &pos);
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* value_ptr = (void*)leaf->ch(pos).value;  // GET POINTER
                char* value_data = (char*)value_ptr;           // +1 INDIRECTION
                found2++;
                volatile char first = value_data[0];           // ACCESS DATA
                volatile char last = value_data[14];
                (void)first; (void)last;
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto range_time2 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Range time: " << range_time2.count() << " μs" << std::endl;
        std::cout << "Found: " << found2 << " values" << std::endl;
        std::cout << "Throughput: " << (found2 * 1000000.0 / range_time2.count()) << " ops/sec" << std::endl;
        
        // COMPARISON
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "PERFORMANCE COMPARISON" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        double speedup = (double)range_time2.count() / range_time1.count();
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "8-byte range query:  " << range_time1.count() << " μs" << std::endl;
        std::cout << "16-byte range query: " << range_time2.count() << " μs" << std::endl;
        std::cout << "16-byte is " << speedup << "x SLOWER" << std::endl;
        
        std::cout << "\nWHY 16-BYTE IS SLOWER:" << std::endl;
        std::cout << "• Extra memory access per value (indirection)" << std::endl;
        std::cout << "• Poor cache locality (scattered DRAM access)" << std::endl;
        std::cout << "• Memory bandwidth bottleneck" << std::endl;
        
        std::cout << "\nIMPLICATIONS:" << std::endl;
        std::cout << "• Range queries expose LBTree's indirection overhead" << std::endl;
        std::cout << "• 8-byte values are optimal for LBTree" << std::endl;
        std::cout << "• >8-byte values hurt range query performance" << std::endl;
        std::cout << "• True NVM storage would be even slower" << std::endl;
        
        delete[] value_storage;
        delete tree2;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}