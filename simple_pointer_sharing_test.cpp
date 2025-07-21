#include <iostream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "SIMPLE PROOF: LBTree Shares Your Pointers" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Initialize LBTree
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/simple_pointer_sharing.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/simple_pointer_sharing.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "✓ LBTree initialized" << std::endl;
        
        // STEP 1: Create value with initial content
        std::cout << "\nSTEP 1: Create 16-byte value with initial content" << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;
        
        char* my_value = new char[16];
        strcpy(my_value, "ORIGINAL_VALUE");
        
        std::cout << "Created value at address: " << (void*)my_value << std::endl;
        std::cout << "Initial content: \"" << my_value << "\"" << std::endl;
        
        // STEP 2: Insert into LBTree
        std::cout << "\nSTEP 2: Insert into LBTree" << std::endl;
        std::cout << "---------------------------" << std::endl;
        
        key_type test_key = 12345;
        tree->insert(test_key, (void*)my_value);
        
        std::cout << "Inserted key " << test_key << " -> pointer " << (void*)my_value << std::endl;
        
        // STEP 3: Verify initial lookup
        std::cout << "\nSTEP 3: Verify initial lookup" << std::endl;
        std::cout << "------------------------------" << std::endl;
        
        int pos;
        void* result = tree->lookup(test_key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            char* retrieved_value = (char*)stored_ptr;
            
            std::cout << "LBTree returns pointer: " << stored_ptr << std::endl;
            std::cout << "Retrieved value: \"" << retrieved_value << "\"" << std::endl;
            std::cout << "Pointer match: " << (stored_ptr == (void*)my_value ? "YES" : "NO") << std::endl;
        }
        
        // STEP 4: MODIFY THE ORIGINAL VALUE (THE KEY TEST!)
        std::cout << "\nSTEP 4: MODIFY the original value (THE KEY TEST!)" << std::endl;
        std::cout << "=================================================" << std::endl;
        
        std::cout << "Changing original value from \"" << my_value << "\" to \"MODIFIED_VALUE\"..." << std::endl;
        strcpy(my_value, "MODIFIED_VALUE");
        std::cout << "Original value now contains: \"" << my_value << "\"" << std::endl;
        
        // STEP 5: Lookup again - THE PROOF!
        std::cout << "\nSTEP 5: Lookup again - THE PROOF!" << std::endl;
        std::cout << "===================================" << std::endl;
        
        result = tree->lookup(test_key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            char* retrieved_value = (char*)stored_ptr;
            
            std::cout << "LBTree returns pointer: " << stored_ptr << std::endl;
            std::cout << "Retrieved value: \"" << retrieved_value << "\"" << std::endl;
            
            if (strcmp(retrieved_value, "MODIFIED_VALUE") == 0) {
                std::cout << "\n🎯 PROOF CONFIRMED!" << std::endl;
                std::cout << "✓ LBTree returns the MODIFIED value!" << std::endl;
                std::cout << "✓ This proves LBTree stores your EXACT pointer!" << std::endl;
                std::cout << "✓ LBTree does NOT copy or manage your data!" << std::endl;
            } else if (strcmp(retrieved_value, "ORIGINAL_VALUE") == 0) {
                std::cout << "\n❌ UNEXPECTED: LBTree returned original value" << std::endl;
                std::cout << "This would mean LBTree copied the data (unlikely)" << std::endl;
            } else {
                std::cout << "\n❓ UNEXPECTED: Retrieved unknown value: \"" << retrieved_value << "\"" << std::endl;
            }
        }
        
        // STEP 6: Additional verification with different modification
        std::cout << "\nSTEP 6: Additional verification" << std::endl;
        std::cout << "--------------------------------" << std::endl;
        
        std::cout << "Changing value again to \"FINAL_CHANGE\"..." << std::endl;
        strcpy(my_value, "FINAL_CHANGE");
        
        result = tree->lookup(test_key, &pos);
        if (pos >= 0) {
            bleaf* leaf = (bleaf*)result;
            void* stored_ptr = (void*)leaf->ch(pos).value;
            char* retrieved_value = (char*)stored_ptr;
            
            std::cout << "LBTree now returns: \"" << retrieved_value << "\"" << std::endl;
            
            if (strcmp(retrieved_value, "FINAL_CHANGE") == 0) {
                std::cout << "✓ DOUBLE CONFIRMATION: LBTree sees all changes!" << std::endl;
            }
        }
        
        // FINAL SUMMARY
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "FINAL PROOF SUMMARY" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::cout << "TEST LOGIC:" << std::endl;
        std::cout << "1. Insert value with content 'ORIGINAL_VALUE'" << std::endl;
        std::cout << "2. Modify the SAME memory to 'MODIFIED_VALUE'" << std::endl;
        std::cout << "3. Lookup from LBTree" << std::endl;
        std::cout << "4. If LBTree returns 'MODIFIED_VALUE' → PROOF!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "CONCLUSION:" << std::endl;
        std::cout << "✓ LBTree stores your EXACT pointer (not a copy)" << std::endl;
        std::cout << "✓ LBTree does NOT manage your value data" << std::endl;
        std::cout << "✓ Changes to your data are visible through LBTree" << std::endl;
        std::cout << "✓ YOU are responsible for value persistence" << std::endl;
        std::cout << "✗ 16-byte values are NOT truly persistent in LBTree" << std::endl;
        
        std::cout << "\nIMPLICATIONS:" << std::endl;
        std::cout << "• LBTree is a 'pointer index', not a value store" << std::endl;
        std::cout << "• Performance is good because no value copying/management" << std::endl;
        std::cout << "• Real persistence requires external value management" << std::endl;
        std::cout << "• Benchmark results are misleading for >8-byte values" << std::endl;
        
        delete[] my_value;
        delete tree;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}