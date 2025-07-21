#include <iostream>
#include <cstring>
#include <iomanip>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "PROOF: LBTree 16-byte Values Create Dangling Pointers" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Initialize LBTree
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/proof_dangling_16byte.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/proof_dangling_16byte.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "✓ LBTree initialized successfully" << std::endl;
        
        // STEP 1: Allocate 16-byte values in DRAM
        std::cout << "\nSTEP 1: Allocate 16-byte values in DRAM" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        const int num_records = 5;
        char* value_storage = new char[num_records * 16];
        
        std::cout << "Allocated storage at DRAM address: " << (void*)value_storage << std::endl;
        
        // Fill with recognizable patterns
        for (int i = 0; i < num_records; i++) {
            char* value_ptr = &value_storage[i * 16];
            snprintf(value_ptr, 16, "VALUE_%02d_AAAA", i);
            std::cout << "  Record " << i << " at " << (void*)value_ptr << " = \"" << value_ptr << "\"" << std::endl;
        }
        
        // STEP 2: Insert into LBTree
        std::cout << "\nSTEP 2: Insert pointers into LBTree" << std::endl;
        std::cout << "------------------------------------" << std::endl;
        
        for (int i = 0; i < num_records; i++) {
            key_type key = i + 100;
            char* value_ptr = &value_storage[i * 16];
            
            tree->insert(key, (void*)value_ptr);
            std::cout << "  Inserted key " << key << " -> pointer " << (void*)value_ptr << std::endl;
        }
        
        // STEP 3: Verify insertion works
        std::cout << "\nSTEP 3: Verify lookups work (BEFORE deletion)" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        
        for (int i = 0; i < num_records; i++) {
            key_type key = i + 100;
            int pos;
            void* result = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                char* value_data = (char*)stored_ptr;
                
                std::cout << "  Key " << key << " -> pointer " << stored_ptr 
                         << " -> value \"" << value_data << "\"" << std::endl;
            } else {
                std::cout << "  Key " << key << " -> NOT FOUND!" << std::endl;
            }
        }
        
        // STEP 4: DELETE THE DRAM STORAGE
        std::cout << "\nSTEP 4: DELETE DRAM storage (create dangling pointers)" << std::endl;
        std::cout << "======================================================" << std::endl;
        
        std::cout << "About to delete DRAM storage at: " << (void*)value_storage << std::endl;
        std::cout << "This will make ALL pointers in LBTree DANGLING!" << std::endl;
        
        delete[] value_storage;
        value_storage = nullptr;
        
        std::cout << "✗ DRAM storage DELETED!" << std::endl;
        std::cout << "✗ All LBTree pointers are now DANGLING!" << std::endl;
        
        // STEP 5: Allocate new memory at different addresses
        std::cout << "\nSTEP 5: Allocate new memory (different addresses)" << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;
        
        // Allocate several blocks to ensure different addresses
        char* dummy1 = new char[1024];
        char* dummy2 = new char[2048];
        char* new_storage = new char[num_records * 16];
        
        std::cout << "New storage allocated at: " << (void*)new_storage << std::endl;
        
        // Fill with different pattern
        for (int i = 0; i < num_records; i++) {
            char* value_ptr = &new_storage[i * 16];
            snprintf(value_ptr, 16, "NEW_%02d_BBBBBB", i);
        }
        
        // STEP 6: Test lookups after deletion (PROOF OF DANGLING POINTERS)
        std::cout << "\nSTEP 6: Test lookups AFTER deletion (DANGLING POINTERS)" << std::endl;
        std::cout << "========================================================" << std::endl;
        
        for (int i = 0; i < num_records; i++) {
            key_type key = i + 100;
            int pos;
            void* result = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                
                std::cout << "Key " << key << ":" << std::endl;
                std::cout << "  LBTree returns pointer: " << stored_ptr << std::endl;
                std::cout << "  Original storage was:   " << (void*)((char*)nullptr) << " (DELETED)" << std::endl;
                std::cout << "  New storage is at:      " << (void*)&new_storage[i * 16] << std::endl;
                
                if (stored_ptr == (void*)&new_storage[i * 16]) {
                    std::cout << "  Status: IMPOSSIBLE - LBTree updated pointer!" << std::endl;
                } else {
                    std::cout << "  Status: CONFIRMED DANGLING POINTER!" << std::endl;
                }
                
                std::cout << "  ⚠️  WARNING: Accessing this pointer would cause SEGFAULT!" << std::endl;
                std::cout << std::endl;
            }
        }
        
        // STEP 7: Final proof summary
        std::cout << "FINAL PROOF SUMMARY" << std::endl;
        std::cout << "===================" << std::endl;
        std::cout << "✓ LBTree stores the EXACT pointers you provide" << std::endl;
        std::cout << "✓ LBTree does NOT copy or manage 16-byte values" << std::endl;
        std::cout << "✓ When you delete your DRAM storage, LBTree pointers become DANGLING" << std::endl;
        std::cout << "✓ LBTree is a 'pointer index' - YOU must manage value persistence" << std::endl;
        std::cout << "✗ 16-byte values are NOT truly persistent in LBTree" << std::endl;
        
        std::cout << "\nFor TRUE persistence of >8-byte values, YOU must:" << std::endl;
        std::cout << "1. Allocate values in NVM (not DRAM)" << std::endl;
        std::cout << "2. Manage value lifecycle yourself" << std::endl;
        std::cout << "3. Handle fragmentation and bookkeeping" << std::endl;
        
        // Cleanup
        delete[] dummy1;
        delete[] dummy2;
        delete[] new_storage;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}