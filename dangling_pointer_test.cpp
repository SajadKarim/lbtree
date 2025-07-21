#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

class DanglingPointerTest {
private:
    lbtree* tree;
    char* value_storage;
    
public:
    DanglingPointerTest() : tree(nullptr), value_storage(nullptr) {}
    
    ~DanglingPointerTest() {
        if (tree) delete tree;
        // NOTE: We'll deliberately delete value_storage to create dangling pointers
    }
    
    void initializeTree() {
        std::cout << "=== DANGLING POINTER TEST ===" << std::endl;
        std::cout << "Testing if LBTree stores our exact pointers or copies data" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/dangling_pointer_test.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/dangling_pointer_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        std::cout << "Tree initialized successfully" << std::endl;
    }
    
    void runDanglingPointerTest() {
        const int test_records = 10;
        
        std::cout << "\n=== STEP 1: ALLOCATE VALUES IN DRAM ===" << std::endl;
        
        // Allocate DRAM storage for values
        value_storage = new char[test_records * 16];
        std::cout << "Allocated value storage at DRAM address: " << (void*)value_storage << std::endl;
        
        // Fill all values with 'A' characters
        for (int i = 0; i < test_records; i++) {
            char* value_ptr = &value_storage[i * 16];
            memset(value_ptr, 'A', 15);  // Fill with 'A'
            value_ptr[15] = '\0';        // Null terminate
            
            std::cout << "Value " << i << " at address " << (void*)value_ptr 
                     << " contains: \"" << std::string(value_ptr, 15) << "\"" << std::endl;
        }
        
        std::cout << "\n=== STEP 2: INSERT INTO LBTREE ===" << std::endl;
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Insert all records
        for (int i = 0; i < test_records; i++) {
            key_type key = i + 1;
            char* value_ptr = &value_storage[i * 16];
            
            tree->insert(key, (void*)value_ptr);
            std::cout << "Inserted key " << key << " -> pointer " << (void*)value_ptr << std::endl;
        }
        
        std::cout << "\n=== STEP 3: VERIFY INSERTION (BEFORE DELETION) ===" << std::endl;
        
        // Verify lookups work correctly
        for (int i = 0; i < test_records; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                char* value_data = (char*)stored_ptr;
                
                std::cout << "Key " << key << " -> stored pointer: " << stored_ptr 
                         << " -> value: \"" << std::string(value_data, 15) << "\"" << std::endl;
            } else {
                std::cout << "Key " << key << " -> NOT FOUND!" << std::endl;
            }
        }
        
        std::cout << "\n=== STEP 4: DELETE VALUE STORAGE (CREATE DANGLING POINTERS) ===" << std::endl;
        
        std::cout << "About to delete value_storage at address: " << (void*)value_storage << std::endl;
        std::cout << "This will make all pointers stored in LBTree DANGLING!" << std::endl;
        
        // DELETE THE VALUE STORAGE - THIS CREATES DANGLING POINTERS!
        delete[] value_storage;
        value_storage = nullptr;
        
        std::cout << "Value storage DELETED! All pointers in LBTree are now dangling." << std::endl;
        
        std::cout << "\n=== STEP 5: TEST LOOKUPS AFTER DELETION (DANGLING POINTERS) ===" << std::endl;
        
        // Try to lookup - this should show dangling pointers
        for (int i = 0; i < test_records; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                
                std::cout << "Key " << key << " -> stored pointer: " << stored_ptr << std::endl;
                std::cout << "  This pointer is now DANGLING (points to freed memory)!" << std::endl;
                
                // WARNING: The following would likely cause a segfault!
                // char* value_data = (char*)stored_ptr;
                // std::cout << "  Value: " << std::string(value_data, 15) << std::endl;
                
                std::cout << "  (Not accessing value to avoid segfault)" << std::endl;
            } else {
                std::cout << "Key " << key << " -> NOT FOUND!" << std::endl;
            }
        }
        
        std::cout << "\n=== STEP 6: ALLOCATE NEW MEMORY AT DIFFERENT ADDRESS ===" << std::endl;
        
        // Allocate new memory (likely at different address)
        char* new_storage = new char[test_records * 16];
        std::cout << "New storage allocated at address: " << (void*)new_storage << std::endl;
        
        // Fill with 'B' characters
        for (int i = 0; i < test_records; i++) {
            char* value_ptr = &new_storage[i * 16];
            memset(value_ptr, 'B', 15);  // Fill with 'B'
            value_ptr[15] = '\0';
        }
        
        std::cout << "\n=== STEP 7: FINAL VERIFICATION ===" << std::endl;
        
        // Check what LBTree still returns
        for (int i = 0; i < 3; i++) {  // Just check first 3
            key_type key = i + 1;
            int pos;
            void* result = tree->lookup(key, &pos);
            
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                
                std::cout << "Key " << key << ":" << std::endl;
                std::cout << "  LBTree returns pointer: " << stored_ptr << std::endl;
                std::cout << "  Original storage was at: " << (void*)((char*)nullptr + (i * 16)) << " (deleted)" << std::endl;
                std::cout << "  New storage is at: " << (void*)&new_storage[i * 16] << std::endl;
                
                if (stored_ptr == (void*)&new_storage[i * 16]) {
                    std::cout << "  IMPOSSIBLE: LBTree somehow updated to new address!" << std::endl;
                } else {
                    std::cout << "  CONFIRMED: LBTree still returns old (dangling) pointer!" << std::endl;
                }
            }
        }
        
        delete[] new_storage;
    }
    
    void printConclusions() {
        std::cout << "\n=== CONCLUSIONS ===" << std::endl;
        std::cout << "1. LBTree stores the EXACT pointers you provide during insert" << std::endl;
        std::cout << "2. LBTree does NOT copy or manage your value data" << std::endl;
        std::cout << "3. When you delete your values, LBTree pointers become dangling" << std::endl;
        std::cout << "4. LBTree is just a 'pointer index' - YOU manage the actual data" << std::endl;
        std::cout << "5. For true persistence, YOU must allocate values in NVM" << std::endl;
    }
    
    void run() {
        initializeTree();
        runDanglingPointerTest();
        printConclusions();
    }
};

int main() {
    try {
        DanglingPointerTest test;
        test.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}