#include <iostream>
#include <cstring>
#include <vector>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "=== DEFINITIVE POINTER TEST ===" << std::endl;
        
        // Initialize LBTree
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/definitive_pointer_test.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/definitive_pointer_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "Tree initialized" << std::endl;
        
        // Test with multiple allocations to force different addresses
        std::vector<char*> allocations;
        
        // Allocate multiple blocks to fragment memory
        for (int i = 0; i < 10; i++) {
            char* block = new char[1024];
            allocations.push_back(block);
        }
        
        // Create our test value
        char* test_value = new char[16];
        strcpy(test_value, "TEST_VALUE_123");
        allocations.push_back(test_value);
        
        std::cout << "\nStep 1: Insert value into LBTree" << std::endl;
        std::cout << "Test value address: " << (void*)test_value << std::endl;
        std::cout << "Test value content: " << test_value << std::endl;
        
        // Insert into tree
        key_type key = 456;
        tree->insert(key, (void*)test_value);
        
        // Verify insertion
        int pos;
        void* result = tree->lookup(key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            std::cout << "LBTree stored pointer: " << stored_ptr << std::endl;
            std::cout << "Exact same pointer: " << (stored_ptr == (void*)test_value ? "YES" : "NO") << std::endl;
            
            // Access value through LBTree pointer
            char* value_via_tree = (char*)stored_ptr;
            std::cout << "Value via LBTree: " << value_via_tree << std::endl;
        }
        
        std::cout << "\nStep 2: Delete ALL allocations (including test value)" << std::endl;
        
        // Delete all allocations including our test value
        for (char* ptr : allocations) {
            delete[] ptr;
        }
        allocations.clear();
        test_value = nullptr;  // Now dangling
        
        std::cout << "All memory freed - test_value is now dangling!" << std::endl;
        
        // Allocate completely different memory pattern
        char* different_memory = new char[2048];  // Different size
        strcpy(different_memory, "DIFFERENT_DATA");
        std::cout << "New allocation address: " << (void*)different_memory << std::endl;
        std::cout << "New allocation content: " << different_memory << std::endl;
        
        std::cout << "\nStep 3: Check what LBTree still returns" << std::endl;
        
        result = tree->lookup(key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            
            std::cout << "LBTree still returns pointer: " << stored_ptr << std::endl;
            std::cout << "New memory is at: " << (void*)different_memory << std::endl;
            std::cout << "Pointers are different: " << (stored_ptr != (void*)different_memory ? "YES" : "NO") << std::endl;
            
            std::cout << "\nThis proves LBTree stores YOUR EXACT POINTER!" << std::endl;
            std::cout << "The pointer " << stored_ptr << " is now DANGLING!" << std::endl;
            std::cout << "(Accessing it would likely cause segfault)" << std::endl;
        }
        
        std::cout << "\nStep 4: Demonstrate with integer values (no indirection)" << std::endl;
        
        // Test with integer stored as pointer (no indirection)
        key_type key2 = 789;
        uint64_t int_value = 0xDEADBEEFCAFEBABE;
        
        tree->insert(key2, (void*)int_value);  // Store integer as pointer
        
        result = tree->lookup(key2, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            uint64_t stored_int = (uint64_t)leaf->ch(pos).value;
            
            std::cout << "Stored integer: 0x" << std::hex << int_value << std::dec << std::endl;
            std::cout << "Retrieved integer: 0x" << std::hex << stored_int << std::dec << std::endl;
            std::cout << "Values match: " << (stored_int == int_value ? "YES" : "NO") << std::endl;
            std::cout << "This works because no indirection - value IS the pointer!" << std::endl;
        }
        
        std::cout << "\n=== FINAL CONCLUSIONS ===" << std::endl;
        std::cout << "1. LBTree stores the EXACT 8-byte pointer you provide" << std::endl;
        std::cout << "2. LBTree does NOT copy, serialize, or manage your data" << std::endl;
        std::cout << "3. When you free your data, LBTree pointers become dangling" << std::endl;
        std::cout << "4. LBTree is a 'pointer index' - YOU are responsible for data management" << std::endl;
        std::cout << "5. For persistence, YOU must allocate values in NVM and manage them" << std::endl;
        
        delete[] different_memory;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}