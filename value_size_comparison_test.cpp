#include <iostream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "=== VALUE SIZE COMPARISON TEST ===" << std::endl;
        std::cout << "Testing 8-byte vs 16-byte value handling in LBTree" << std::endl;
        
        // Initialize LBTree
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/value_size_test.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/value_size_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "Tree initialized" << std::endl;
        
        // Test 1: 8-byte values (stored as pointer directly)
        std::cout << "\n=== TEST 1: 8-BYTE VALUES (NO INDIRECTION) ===" << std::endl;
        
        key_type key1 = 100;
        uint64_t value_8byte = 0xDEADBEEFCAFEBABE;
        
        std::cout << "Inserting 8-byte value: 0x" << std::hex << value_8byte << std::dec << std::endl;
        std::cout << "Method: Store value directly as pointer (no indirection)" << std::endl;
        
        // Store the 8-byte value directly as a pointer
        tree->insert(key1, (void*)value_8byte);
        
        // Lookup
        int pos;
        void* result = tree->lookup(key1, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            uint64_t retrieved_value = (uint64_t)leaf->ch(pos).value;
            
            std::cout << "Retrieved value: 0x" << std::hex << retrieved_value << std::dec << std::endl;
            std::cout << "Values match: " << (retrieved_value == value_8byte ? "YES" : "NO") << std::endl;
            std::cout << "Storage: Value IS the pointer (no external storage needed)" << std::endl;
        }
        
        // Test 2: 16-byte values (requires external storage)
        std::cout << "\n=== TEST 2: 16-BYTE VALUES (WITH INDIRECTION) ===" << std::endl;
        
        key_type key2 = 200;
        char* value_16byte = new char[16];
        strncpy(value_16byte, "FIFTEEN_BYTE_VAL", 15);
        value_16byte[15] = '\0';
        
        std::cout << "Inserting 16-byte value: \"" << value_16byte << "\"" << std::endl;
        std::cout << "Value address: " << (void*)value_16byte << std::endl;
        std::cout << "Method: Store pointer to external DRAM location" << std::endl;
        
        // Store pointer to the 16-byte value
        tree->insert(key2, (void*)value_16byte);
        
        // Lookup
        result = tree->lookup(key2, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            
            std::cout << "Retrieved pointer: " << stored_ptr << std::endl;
            std::cout << "Pointers match: " << (stored_ptr == (void*)value_16byte ? "YES" : "NO") << std::endl;
            
            // Access the actual value through indirection
            char* actual_value = (char*)stored_ptr;
            std::cout << "Value through indirection: \"" << actual_value << "\"" << std::endl;
            std::cout << "Storage: Pointer to external DRAM (indirection required)" << std::endl;
        }
        
        // Test 3: What happens when we delete the 16-byte value?
        std::cout << "\n=== TEST 3: DELETE 16-BYTE VALUE (CREATE DANGLING POINTER) ===" << std::endl;
        
        std::cout << "Deleting 16-byte value storage..." << std::endl;
        delete[] value_16byte;
        value_16byte = nullptr;
        
        // Try lookup again
        result = tree->lookup(key2, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            
            std::cout << "LBTree still returns pointer: " << stored_ptr << std::endl;
            std::cout << "This pointer is now DANGLING!" << std::endl;
            std::cout << "(Accessing it would cause segfault)" << std::endl;
        }
        
        // Test 4: 8-byte value is unaffected
        std::cout << "\n=== TEST 4: 8-BYTE VALUE STILL WORKS ===" << std::endl;
        
        result = tree->lookup(key1, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            uint64_t retrieved_value = (uint64_t)leaf->ch(pos).value;
            
            std::cout << "8-byte value still works: 0x" << std::hex << retrieved_value << std::dec << std::endl;
            std::cout << "No dangling pointer because value IS the pointer!" << std::endl;
        }
        
        std::cout << "\n=== CONCLUSIONS ===" << std::endl;
        std::cout << "1. 8-BYTE VALUES: Stored directly as pointer (no indirection)" << std::endl;
        std::cout << "   - No external storage needed" << std::endl;
        std::cout << "   - No dangling pointer issues" << std::endl;
        std::cout << "   - Perfect for integers, IDs, timestamps" << std::endl;
        std::cout << "   - Truly persistent (value is in the tree)" << std::endl;
        
        std::cout << "\n2. >8-BYTE VALUES: Stored as pointer to external memory" << std::endl;
        std::cout << "   - Requires external storage management" << std::endl;
        std::cout << "   - Subject to dangling pointer issues" << std::endl;
        std::cout << "   - YOU must manage persistence" << std::endl;
        std::cout << "   - LBTree just stores the pointer" << std::endl;
        
        std::cout << "\n3. PERFORMANCE IMPLICATIONS:" << std::endl;
        std::cout << "   - 8-byte: No indirection overhead" << std::endl;
        std::cout << "   - >8-byte: +1 memory access per lookup" << std::endl;
        
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}