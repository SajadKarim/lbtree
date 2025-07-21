#include <iostream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "SINGLE VALUE ACCESS TEST" << std::endl;
        std::cout << "========================================" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 50 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/single_value_test.nvm";
        long long nvm_size = 512 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/single_value_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "✓ LBTree initialized" << std::endl;
        
        // Create and insert ONE 16-byte value
        char* my_value = new char[16];
        strcpy(my_value, "TEST_VALUE_123");
        
        key_type test_key = 12345;
        tree->insert(test_key, (void*)my_value);
        
        std::cout << "✓ Inserted key " << test_key << " -> pointer " << (void*)my_value << std::endl;
        std::cout << "✓ Original value: \"" << my_value << "\"" << std::endl;
        
        // TEST 1: Get pointer only (what original benchmark did)
        std::cout << "\n=== TEST 1: POINTER-ONLY ACCESS ===" << std::endl;
        
        int pos;
        void* result = tree->lookup(test_key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            
            std::cout << "✓ LBTree returned pointer: " << stored_ptr << std::endl;
            std::cout << "✓ Pointer match: " << (stored_ptr == (void*)my_value ? "YES" : "NO") << std::endl;
        } else {
            std::cout << "❌ Lookup failed!" << std::endl;
            return 1;
        }
        
        // TEST 2: Actually access the value (what crashes in bulk)
        std::cout << "\n=== TEST 2: SINGLE VALUE ACCESS ===" << std::endl;
        
        result = tree->lookup(test_key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            
            std::cout << "Attempting to access value through pointer..." << std::endl;
            
            try {
                char* actual_value = (char*)stored_ptr;
                std::cout << "✓ Got char pointer: " << (void*)actual_value << std::endl;
                
                // Try to read first byte
                std::cout << "Attempting to read first byte..." << std::endl;
                volatile char first_byte = actual_value[0];
                std::cout << "✓ First byte: '" << first_byte << "' (ASCII " << (int)first_byte << ")" << std::endl;
                
                // Try to read the full string
                std::cout << "Attempting to read full value..." << std::endl;
                std::cout << "✓ Retrieved value: \"" << actual_value << "\"" << std::endl;
                
                // Try to read last byte
                std::cout << "Attempting to read last byte..." << std::endl;
                volatile char last_byte = actual_value[15];
                std::cout << "✓ Last byte: '" << last_byte << "' (ASCII " << (int)last_byte << ")" << std::endl;
                
                std::cout << "\n🎉 SUCCESS: Single value access works!" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "❌ Exception during value access: " << e.what() << std::endl;
            }
        }
        
        // TEST 3: Multiple single accesses (to see where it breaks)
        std::cout << "\n=== TEST 3: MULTIPLE SINGLE ACCESSES ===" << std::endl;
        
        for (int i = 0; i < 10; i++) {
            result = tree->lookup(test_key, &pos);
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                char* actual_value = (char*)stored_ptr;
                
                volatile char test_byte = actual_value[0];
                std::cout << "Access " << (i+1) << ": ✓ First byte = '" << test_byte << "'" << std::endl;
                (void)test_byte;
            } else {
                std::cout << "Access " << (i+1) << ": ❌ Lookup failed" << std::endl;
                break;
            }
        }
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "ANALYSIS" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::cout << "If single access works but bulk access crashes:" << std::endl;
        std::cout << "• Problem is likely in bulk processing" << std::endl;
        std::cout << "• Memory corruption during large operations" << std::endl;
        std::cout << "• Pointer invalidation at scale" << std::endl;
        std::cout << "• System resource exhaustion" << std::endl;
        
        delete[] my_value;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}