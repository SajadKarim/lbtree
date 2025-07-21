#include <iostream>
#include <cstring>
#include <vector>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "POINTER VALIDITY TEST" << std::endl;
        std::cout << "========================================" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 50 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/pointer_validity_test.nvm";
        long long nvm_size = 512 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/pointer_validity_test.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        lbtree* tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        std::cout << "✓ LBTree initialized" << std::endl;
        
        // Test with different scales to find breaking point
        std::vector<int> test_sizes = {1, 10, 100, 1000, 10000, 100000};
        
        for (int test_size : test_sizes) {
            std::cout << "\n" << std::string(50, '=') << std::endl;
            std::cout << "TESTING " << test_size << " RECORDS" << std::endl;
            std::cout << std::string(50, '=') << std::endl;
            
            std::vector<char*> my_pointers;
            std::vector<key_type> keys;
            
            // Insert phase
            std::cout << "Inserting " << test_size << " records..." << std::endl;
            for (int i = 0; i < test_size; i++) {
                key_type key = 1000000 + i;  // Use unique keys
                char* value = new char[16];
                sprintf(value, "VALUE_%06d", i);
                
                my_pointers.push_back(value);
                keys.push_back(key);
                
                tree->insert(key, (void*)value);
            }
            std::cout << "✓ Insert completed" << std::endl;
            
            // Lookup and pointer comparison phase
            std::cout << "Testing pointer retrieval..." << std::endl;
            int pointer_matches = 0;
            for (int i = 0; i < test_size; i++) {
                key_type key = keys[i];
                int pos;
                void* result = tree->lookup(key, &pos);
                if (pos >= 0) {
                    bleaf* leaf = (bleaf*)result;
                    void* lbtree_ptr = (void*)leaf->ch(pos).value;
                    
                    if (lbtree_ptr == (void*)my_pointers[i]) {
                        pointer_matches++;
                    }
                }
            }
            std::cout << "✓ Pointer matches: " << pointer_matches << "/" << test_size << std::endl;
            
            // CRITICAL: Content access phase
            std::cout << "Testing content access..." << std::endl;
            int content_matches = 0;
            bool access_failed = false;
            
            for (int i = 0; i < test_size && !access_failed; i++) {
                key_type key = keys[i];
                int pos;
                void* result = tree->lookup(key, &pos);
                if (pos >= 0) {
                    bleaf* leaf = (bleaf*)result;
                    void* lbtree_ptr = (void*)leaf->ch(pos).value;
                    
                    try {
                        // Try to access the content
                        char* actual_value = (char*)lbtree_ptr;
                        char* expected_value = my_pointers[i];
                        
                        // Test single byte access first
                        volatile char test_byte = actual_value[0];
                        (void)test_byte;
                        
                        // Test full content comparison
                        if (memcmp(actual_value, expected_value, 16) == 0) {
                            content_matches++;
                        }
                        
                        // Progress indicator for large datasets
                        if (i % 1000 == 0) {
                            std::cout << "  Processed " << i << "/" << test_size << " records..." << std::endl;
                        }
                        
                    } catch (const std::exception& e) {
                        std::cout << "❌ Exception at record " << i << ": " << e.what() << std::endl;
                        access_failed = true;
                        break;
                    }
                }
            }
            
            if (!access_failed) {
                std::cout << "✓ Content access completed: " << content_matches << "/" << test_size << " matches" << std::endl;
            } else {
                std::cout << "❌ Content access FAILED at scale " << test_size << std::endl;
            }
            
            // Cleanup
            for (char* ptr : my_pointers) {
                delete[] ptr;
            }
            
            // Clear tree for next test
            for (key_type key : keys) {
                tree->del(key);
            }
            
            if (access_failed) {
                std::cout << "\n🚨 BREAKING POINT FOUND: " << test_size << " records" << std::endl;
                break;
            }
        }
        
        delete tree;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}