#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <numeric>
#include <algorithm>
#include <random>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "LOOKUP COMPARISON: Pointer vs Value Access" << std::endl;
        std::cout << "========================================" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/lookup_comparison.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/lookup_comparison.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        const size_t NUM_RECORDS = 50000;  // Smaller to avoid crashes
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Allocate and insert 16-byte values
        char* value_storage = new char[NUM_RECORDS * 16];
        std::cout << "Inserting " << NUM_RECORDS << " 16-byte values..." << std::endl;
        
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = i + 1;
            char* value_ptr = &value_storage[i * 16];
            
            // Create recognizable 16-byte value
            memset(value_ptr, 0, 16);
            memcpy(value_ptr, &key, sizeof(key_type));
            for (int j = 8; j < 16; j++) {
                value_ptr[j] = (char)((key >> ((j-8) * 8)) & 0xFF);
            }
            
            tree->insert(key, (void*)value_ptr);
        }
        
        // Generate lookup keys
        std::vector<uint64_t> lookup_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, lookup_data);
        
        // TEST 1: Only get pointers (what the original benchmark did)
        std::cout << "\n=== TEST 1: POINTER-ONLY LOOKUP (MISLEADING) ===" << std::endl;
        std::cout << "Only measure getting the pointer, not accessing value" << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        int found1 = 0;
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = (key_type)lookup_data[i];
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                found1++;
                // OLD WAY: Only get pointer, don't access value
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;  // Just get pointer
                (void)stored_ptr;  // Don't actually use it
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Time: " << duration1.count() / 1000.0 << " ms" << std::endl;
        std::cout << "Throughput: " << (found1 * 1000000.0 / duration1.count()) << " ops/sec" << std::endl;
        std::cout << "Found: " << found1 << " records" << std::endl;
        
        // TEST 2: Actually access the 16-byte values (what we should measure)
        std::cout << "\n=== TEST 2: FULL VALUE ACCESS (REALISTIC) ===" << std::endl;
        std::cout << "Actually access the 16-byte values through indirection" << std::endl;
        
        start = std::chrono::high_resolution_clock::now();
        
        int found2 = 0;
        int value_matches = 0;
        for (size_t i = 0; i < NUM_RECORDS; i++) {
            key_type key = (key_type)lookup_data[i];
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                found2++;
                
                // NEW WAY: Actually access the 16-byte value
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;  // Get pointer
                char* actual_value = (char*)stored_ptr;         // +1 indirection!
                
                // Verify the value (forces memory access)
                if (actual_value != nullptr) {
                    key_type stored_key;
                    memcpy(&stored_key, actual_value, sizeof(key_type));
                    if (stored_key == key) {
                        value_matches++;
                    }
                    
                    // Force access to entire 16-byte value
                    volatile char first = actual_value[0];
                    volatile char middle = actual_value[8];
                    volatile char last = actual_value[15];
                    (void)first; (void)middle; (void)last;
                }
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Time: " << duration2.count() / 1000.0 << " ms" << std::endl;
        std::cout << "Throughput: " << (found2 * 1000000.0 / duration2.count()) << " ops/sec" << std::endl;
        std::cout << "Found: " << found2 << " records" << std::endl;
        std::cout << "Value matches: " << value_matches << " records" << std::endl;
        
        // COMPARISON
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PERFORMANCE COMPARISON" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        double slowdown = (double)duration2.count() / duration1.count();
        double throughput1 = found1 * 1000000.0 / duration1.count();
        double throughput2 = found2 * 1000000.0 / duration2.count();
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Pointer-only lookup:  " << duration1.count() / 1000.0 << " ms (" << std::setprecision(0) << throughput1 << " ops/sec)" << std::endl;
        std::cout << "Full value access:    " << std::setprecision(2) << duration2.count() / 1000.0 << " ms (" << std::setprecision(0) << throughput2 << " ops/sec)" << std::endl;
        std::cout << "Slowdown: " << std::setprecision(2) << slowdown << "x" << std::endl;
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "ANALYSIS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << "\n✓ WHAT THE ORIGINAL BENCHMARK MEASURED:" << std::endl;
        std::cout << "  • Tree traversal + pointer retrieval" << std::endl;
        std::cout << "  • NO actual value access" << std::endl;
        std::cout << "  • MISLEADING performance numbers" << std::endl;
        
        std::cout << "\n✓ WHAT REALISTIC APPLICATIONS NEED:" << std::endl;
        std::cout << "  • Tree traversal + pointer retrieval + value access" << std::endl;
        std::cout << "  • +1 memory indirection per lookup" << std::endl;
        std::cout << "  • REALISTIC performance numbers" << std::endl;
        
        std::cout << "\n✓ THE HIDDEN COST:" << std::endl;
        std::cout << "  • Extra memory access: " << std::setprecision(2) << slowdown << "x slower" << std::endl;
        std::cout << "  • Cache misses from scattered DRAM access" << std::endl;
        std::cout << "  • Memory bandwidth consumption" << std::endl;
        
        std::cout << "\n✓ IMPLICATIONS:" << std::endl;
        std::cout << "  • Original benchmark was measuring wrong thing" << std::endl;
        std::cout << "  • Real applications will be " << slowdown << "x slower" << std::endl;
        std::cout << "  • Range queries will be even worse" << std::endl;
        std::cout << "  • LBTree's pointer approach has hidden costs" << std::endl;
        
        delete[] value_storage;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}