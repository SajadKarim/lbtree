#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

// External function declaration
extern void initUseful(void);

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class TreeStructureAnalyzer {
private:
    lbtree* tree;
    char* value_storage_16;
    
public:
    TreeStructureAnalyzer() : tree(nullptr), value_storage_16(nullptr) {}
    
    ~TreeStructureAnalyzer() {
        if (tree) delete tree;
        if (value_storage_16) delete[] value_storage_16;
    }
    
    void initializeTree() {
        std::cout << "=== TREE STRUCTURE ANALYZER ===" << std::endl;
        std::cout << "Analyzing LBTree node structure and memory layout" << std::endl;
        
        // Print compile-time constants
        std::cout << "\n=== COMPILE-TIME CONSTANTS ===" << std::endl;
        std::cout << "LEAF_KEY_NUM: " << LEAF_KEY_NUM << std::endl;
        std::cout << "NON_LEAF_KEY_NUM: " << NON_LEAF_KEY_NUM << std::endl;
        std::cout << "LEAF_SIZE: " << LEAF_SIZE << " bytes" << std::endl;
        std::cout << "NONLEAF_SIZE: " << NONLEAF_SIZE << " bytes" << std::endl;
        std::cout << "KEY_SIZE: " << KEY_SIZE << " bytes" << std::endl;
        std::cout << "POINTER_SIZE: " << POINTER_SIZE << " bytes" << std::endl;
        
        // Calculate actual sizes
        std::cout << "\n=== CALCULATED SIZES ===" << std::endl;
        std::cout << "IdxEntry size: " << sizeof(IdxEntry) << " bytes (key + pointer)" << std::endl;
        std::cout << "bleaf size: " << sizeof(bleaf) << " bytes" << std::endl;
        std::cout << "bnode size: " << sizeof(bnode) << " bytes" << std::endl;
        
        // Analyze leaf structure
        std::cout << "\n=== LEAF NODE ANALYSIS ===" << std::endl;
        std::cout << "Leaf header: 16 bytes (bitmap + lock + alt + fingerprints)" << std::endl;
        std::cout << "Leaf entries: " << LEAF_KEY_NUM << " x " << sizeof(IdxEntry) << " = " << (LEAF_KEY_NUM * sizeof(IdxEntry)) << " bytes" << std::endl;
        std::cout << "Leaf sibling pointers: 2 x 8 = 16 bytes" << std::endl;
        std::cout << "Total leaf size: " << (16 + LEAF_KEY_NUM * sizeof(IdxEntry) + 16) << " bytes" << std::endl;
        
        // Initialize useful structures first
        initUseful();
        
        // Initialize worker thread (required)
        worker_thread_num = 1;
        worker_id = 0;
        
        // Initialize memory pool (100MB)
        long long mem_size = 100 * 1024 * 1024; // 100MB
        the_thread_mempools.init(1, mem_size, 4096);
        
        // Initialize NVM pool - Use Optane PMEM
        const char* nvm_filename = "/mnt/tmpfs/tree_structure_analyzer.nvm";
        long long nvm_size = 1024 * 1024 * 1024; // 1GB
        
        // Clean up any existing NVM file
        std::system("rm -f /mnt/tmpfs/tree_structure_analyzer.nvm");
        
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        // Allocate tree metadata in NVM
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024); // 4KB
        tree = new lbtree(nvm_addr, false);
        
        // Initialize NVM logging
        nvmLogInit(1);
        
        // Allocate storage for 16-byte values
        value_storage_16 = new char[1000000 * 16];
        
        std::cout << "\nLBTree initialized successfully" << std::endl;
    }
    
    void createValue16(key_type key, char* value) {
        memset(value, 0, 16);
        memcpy(value, &key, sizeof(key_type));
        for (int i = 8; i < 16; i++) {
            value[i] = (char)((key >> ((i-8) * 8)) & 0xFF);
        }
    }
    
    void analyzeTreeStructure() {
        std::cout << "\n=== TREE STRUCTURE ANALYSIS ===" << std::endl;
        
        // Test with different numbers of records to see tree growth
        std::vector<int> test_sizes = {100, 1000, 10000, 100000};
        
        for (int test_size : test_sizes) {
            std::cout << "\n--- Testing with " << test_size << " records ---" << std::endl;
            
            // Clear tree
            delete tree;
            char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
            tree = new lbtree(nvm_addr, false);
            
            // Generate random keys
            std::vector<uint64_t> insert_data;
            GENERATE_RANDOM_NUMBER_ARRAY(1, test_size + 1, insert_data);
            
            // Create minimal bulkload
            inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
            int level = tree->bulkload(1, input, 1.0);
            delete input;
            
            // Insert records with 16-byte values
            for (int i = 0; i < test_size; i++) {
                key_type key = (key_type)insert_data[i];
                char* value_ptr = &value_storage_16[i * 16];
                createValue16(key, value_ptr);
                tree->insert(key, (void*)value_ptr);
            }
            
            // Analyze tree structure
            std::cout << "Tree height: " << tree->level() << " levels" << std::endl;
            
            // Test a few lookups to see leaf utilization
            int sample_lookups = (10 < test_size) ? 10 : test_size;
            int total_leaf_entries = 0;
            int leaves_sampled = 0;
            
            for (int i = 0; i < sample_lookups; i++) {
                key_type key = (key_type)insert_data[i];
                int pos;
                void* result = tree->lookup(key, &pos);
                if (pos >= 0) {
                    bleaf* leaf = (bleaf*)result;
                    int leaf_entries = leaf->num();
                    total_leaf_entries += leaf_entries;
                    leaves_sampled++;
                    
                    if (i < 3) { // Show details for first few
                        std::cout << "  Leaf sample " << (i+1) << ": " << leaf_entries << " entries (out of " << LEAF_KEY_NUM << " max)" << std::endl;
                    }
                }
            }
            
            if (leaves_sampled > 0) {
                double avg_leaf_utilization = (double)total_leaf_entries / leaves_sampled;
                double utilization_percent = (avg_leaf_utilization / LEAF_KEY_NUM) * 100.0;
                std::cout << "  Average leaf utilization: " << std::fixed << std::setprecision(1) 
                         << avg_leaf_utilization << "/" << LEAF_KEY_NUM 
                         << " (" << utilization_percent << "%)" << std::endl;
            }
        }
    }
    
    void compareValueStorageImpact() {
        std::cout << "\n=== VALUE STORAGE IMPACT ANALYSIS ===" << std::endl;
        
        const int test_records = 10000;
        
        // Test 1: 8-byte pointer values (traditional)
        std::cout << "\n--- 8-byte pointer values ---" << std::endl;
        delete tree;
        char *nvm_addr1 = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr1, false);
        
        std::vector<uint64_t> insert_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, test_records + 1, insert_data);
        
        inMemKeyInput *input1 = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input1, 1.0);
        delete input1;
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < test_records; i++) {
            key_type key = (key_type)insert_data[i];
            tree->insert(key, (void*)key); // Store key as pointer
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Insert time: " << (duration1.count() / 1000.0) << " ms" << std::endl;
        std::cout << "Tree height: " << tree->level() << " levels" << std::endl;
        
        // Test 2: 16-byte external values
        std::cout << "\n--- 16-byte external values ---" << std::endl;
        delete tree;
        char *nvm_addr2 = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr2, false);
        
        inMemKeyInput *input2 = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input2, 1.0);
        delete input2;
        
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < test_records; i++) {
            key_type key = (key_type)insert_data[i];
            char* value_ptr = &value_storage_16[i * 16];
            createValue16(key, value_ptr);
            tree->insert(key, (void*)value_ptr); // Store pointer to 16-byte value
        }
        end = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Insert time: " << (duration2.count() / 1000.0) << " ms" << std::endl;
        std::cout << "Tree height: " << tree->level() << " levels" << std::endl;
        
        // Compare
        std::cout << "\n--- COMPARISON ---" << std::endl;
        std::cout << "Performance difference: " << std::fixed << std::setprecision(1) 
                 << ((double)duration2.count() / duration1.count() - 1.0) * 100.0 << "%" << std::endl;
        
        std::cout << "\n=== KEY INSIGHT ===" << std::endl;
        std::cout << "Both approaches store the SAME THING in tree nodes:" << std::endl;
        std::cout << "- 8-byte key + 8-byte pointer = 16 bytes per entry" << std::endl;
        std::cout << "- Tree structure is IDENTICAL regardless of value size!" << std::endl;
        std::cout << "- Performance difference comes from:" << std::endl;
        std::cout << "  1. Memory allocation overhead (16-byte values)" << std::endl;
        std::cout << "  2. Cache effects when accessing external values" << std::endl;
        std::cout << "  3. Memory bandwidth utilization" << std::endl;
    }
    
    void run() {
        initializeTree();
        analyzeTreeStructure();
        compareValueStorageImpact();
    }
};

int main() {
    try {
        TreeStructureAnalyzer analyzer;
        analyzer.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Analysis failed: " << e.what() << std::endl;
        return 1;
    }
}