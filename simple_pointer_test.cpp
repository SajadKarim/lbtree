#include <iostream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "=== SIMPLE POINTER TEST ===" << std::endl;
        
        // Initialize LBTree
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/simple_pointer_test.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/simple_pointer_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "Tree initialized" << std::endl;
        
        // Test 1: Create value in DRAM
        char* original_value = new char[16];
        strcpy(original_value, "ORIGINAL_DATA");
        
        std::cout << "\nStep 1: Insert with DRAM pointer" << std::endl;
        std::cout << "Original value address: " << (void*)original_value << std::endl;
        std::cout << "Original value content: " << original_value << std::endl;
        
        // Insert into tree
        key_type key = 123;
        tree->insert(key, (void*)original_value);
        
        // Lookup immediately
        int pos;
        void* result = tree->lookup(key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            std::cout << "LBTree stored pointer: " << stored_ptr << std::endl;
            std::cout << "Pointers match: " << (stored_ptr == (void*)original_value ? "YES" : "NO") << std::endl;
        }
        
        // Test 2: Modify original value
        std::cout << "\nStep 2: Modify original value" << std::endl;
        strcpy(original_value, "MODIFIED_DATA");
        std::cout << "Modified value content: " << original_value << std::endl;
        
        // Lookup again - should see modified data if LBTree stores pointer
        result = tree->lookup(key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            char* value_data = (char*)stored_ptr;
            std::cout << "LBTree returns pointer: " << stored_ptr << std::endl;
            std::cout << "Value through LBTree: " << value_data << std::endl;
            std::cout << "Value changed: " << (strcmp(value_data, "MODIFIED_DATA") == 0 ? "YES" : "NO") << std::endl;
        }
        
        // Test 3: Delete original and allocate new at different address
        std::cout << "\nStep 3: Delete original and allocate new" << std::endl;
        delete[] original_value;
        
        char* new_value = new char[16];
        strcpy(new_value, "NEW_ALLOCATION");
        std::cout << "New value address: " << (void*)new_value << std::endl;
        std::cout << "New value content: " << new_value << std::endl;
        
        // Check what LBTree still returns
        result = tree->lookup(key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            std::cout << "LBTree still returns: " << stored_ptr << std::endl;
            std::cout << "This is now a DANGLING POINTER!" << std::endl;
            std::cout << "New allocation is at: " << (void*)new_value << std::endl;
            std::cout << "Addresses different: " << (stored_ptr != (void*)new_value ? "YES" : "NO") << std::endl;
        }
        
        std::cout << "\n=== CONCLUSION ===" << std::endl;
        std::cout << "LBTree stores the EXACT pointer you provide!" << std::endl;
        std::cout << "It does NOT copy or manage your data!" << std::endl;
        std::cout << "When you delete your data, LBTree pointers become dangling!" << std::endl;
        
        delete[] new_value;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}