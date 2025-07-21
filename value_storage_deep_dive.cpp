#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <cstring>

// Include LBTree headers
#include "lbtree-src/lbtree.h"

extern void initUseful(void);

class ValueStorageDeepDive {
private:
    lbtree* tree;
    
public:
    ValueStorageDeepDive() : tree(nullptr) {}
    
    ~ValueStorageDeepDive() {
        if (tree) delete tree;
    }
    
    void initializeTree() {
        std::cout << "=== VALUE STORAGE DEEP DIVE ANALYSIS ===" << std::endl;
        
        initUseful();
        worker_thread_num = 1;
        worker_id = 0;
        
        long long mem_size = 100 * 1024 * 1024;
        the_thread_mempools.init(1, mem_size, 4096);
        
        const char* nvm_filename = "/mnt/tmpfs/value_storage_deep_dive.nvm";
        long long nvm_size = 1024 * 1024 * 1024;
        std::system("rm -f /mnt/tmpfs/value_storage_deep_dive.nvm");
        the_thread_nvmpools.init(1, nvm_filename, nvm_size);
        
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr, false);
        nvmLogInit(1);
        
        std::cout << "Initialized successfully" << std::endl;
    }
    
    void analyzeValueStorageOptions() {
        std::cout << "\n=== VALUE STORAGE OPTIONS ANALYSIS ===" << std::endl;
        
        std::cout << "\n1. WHAT WE'RE ACTUALLY DOING (Current Implementation):" << std::endl;
        std::cout << "   - Values stored in: DRAM array (char* value_storage)" << std::endl;
        std::cout << "   - Tree stores: 8-byte pointers to DRAM locations" << std::endl;
        std::cout << "   - Persistence: VALUES ARE NOT PERSISTENT!" << std::endl;
        std::cout << "   - Fragmentation: None (contiguous array)" << std::endl;
        std::cout << "   - Bookkeeping: Simple array indexing" << std::endl;
        
        std::cout << "\n2. WHAT SHOULD BE DONE (Proper Persistent Storage):" << std::endl;
        std::cout << "   - Values stored in: NVM (Optane PMEM)" << std::endl;
        std::cout << "   - Tree stores: 8-byte pointers to NVM locations" << std::endl;
        std::cout << "   - Persistence: Both tree AND values persistent" << std::endl;
        std::cout << "   - Fragmentation: Potential issue with variable sizes" << std::endl;
        std::cout << "   - Bookkeeping: Need allocator/free list management" << std::endl;
        
        std::cout << "\n3. ALTERNATIVE APPROACHES:" << std::endl;
        std::cout << "   a) Embedded values: Store values directly in tree nodes" << std::endl;
        std::cout << "   b) Value pages: Allocate fixed-size pages for values" << std::endl;
        std::cout << "   c) Log-structured: Append-only value storage" << std::endl;
        std::cout << "   d) External store: Separate key-value store" << std::endl;
    }
    
    void demonstrateCurrentApproach() {
        std::cout << "\n=== CURRENT APPROACH DEMONSTRATION ===" << std::endl;
        
        const int test_records = 1000;
        
        // Allocate DRAM storage for values (NOT PERSISTENT!)
        char* dram_value_storage = new char[test_records * 16];
        std::cout << "\nAllocated " << (test_records * 16) << " bytes in DRAM for values" << std::endl;
        std::cout << "DRAM storage address: " << (void*)dram_value_storage << std::endl;
        
        // Initialize tree
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Insert records
        std::cout << "\nInserting records..." << std::endl;
        for (int i = 0; i < test_records; i++) {
            key_type key = i + 1;
            
            // Create value in DRAM
            char* value_ptr = &dram_value_storage[i * 16];
            memset(value_ptr, 0, 16);
            memcpy(value_ptr, &key, sizeof(key_type));
            sprintf(value_ptr + 8, "val_%04d", (int)key);
            
            // Store DRAM pointer in tree (tree is in NVM, but points to DRAM!)
            tree->insert(key, (void*)value_ptr);
            
            if (i < 5) {
                std::cout << "  Key " << key << " -> DRAM address " << (void*)value_ptr 
                         << " (value: " << std::string(value_ptr + 8, 8) << ")" << std::endl;
            }
        }
        
        // Demonstrate the problem
        std::cout << "\n=== THE FUNDAMENTAL PROBLEM ===" << std::endl;
        std::cout << "Tree (in NVM): Persistent ✓" << std::endl;
        std::cout << "Values (in DRAM): NOT persistent ✗" << std::endl;
        std::cout << "After restart: Tree exists, but all value pointers are INVALID!" << std::endl;
        
        // Lookup demonstration
        std::cout << "\nLookup demonstration:" << std::endl;
        for (int i = 0; i < 3; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                std::cout << "  Key " << key << " -> stored pointer: " << stored_ptr << std::endl;
                
                // This works NOW, but would crash after restart!
                char* value_data = (char*)stored_ptr;
                std::cout << "    Value: " << std::string(value_data + 8, 8) << std::endl;
            }
        }
        
        delete[] dram_value_storage;
    }
    
    void demonstrateProperNVMStorage() {
        std::cout << "\n=== PROPER NVM VALUE STORAGE APPROACH ===" << std::endl;
        
        const int test_records = 1000;
        const int value_size = 16;
        
        // Allocate NVM storage for values (PERSISTENT!)
        char* nvm_value_storage = (char*)nvmpool_alloc(test_records * value_size);
        std::cout << "\nAllocated " << (test_records * value_size) << " bytes in NVM for values" << std::endl;
        std::cout << "NVM storage address: " << (void*)nvm_value_storage << std::endl;
        
        // Clear tree
        delete tree;
        char *nvm_addr = (char *)nvmpool_alloc(4 * 1024);
        tree = new lbtree(nvm_addr, false);
        
        inMemKeyInput *input = new inMemKeyInput(2, 1, 2);
        tree->bulkload(1, input, 1.0);
        delete input;
        
        // Insert records with NVM value storage
        std::cout << "\nInserting records with NVM value storage..." << std::endl;
        for (int i = 0; i < test_records; i++) {
            key_type key = i + 1;
            
            // Create value in NVM
            char* value_ptr = &nvm_value_storage[i * value_size];
            memset(value_ptr, 0, value_size);
            memcpy(value_ptr, &key, sizeof(key_type));
            sprintf(value_ptr + 8, "nvm_%04d", (int)key);
            
            // Flush to NVM (ensure persistence)
            clwb(value_ptr);
            
            // Store NVM pointer in tree (both tree and values in NVM!)
            tree->insert(key, (void*)value_ptr);
            
            if (i < 5) {
                std::cout << "  Key " << key << " -> NVM address " << (void*)value_ptr 
                         << " (value: " << std::string(value_ptr + 8, 8) << ")" << std::endl;
            }
        }
        
        sfence(); // Ensure all writes are persistent
        
        std::cout << "\n=== PROPER PERSISTENT STORAGE ===" << std::endl;
        std::cout << "Tree (in NVM): Persistent ✓" << std::endl;
        std::cout << "Values (in NVM): Persistent ✓" << std::endl;
        std::cout << "After restart: Both tree and values survive!" << std::endl;
        
        // Lookup demonstration
        std::cout << "\nLookup demonstration:" << std::endl;
        for (int i = 0; i < 3; i++) {
            key_type key = i + 1;
            int pos;
            void* result = tree->lookup(key, &pos);
            if (pos >= 0) {
                bleaf* leaf = (bleaf*)result;
                void* stored_ptr = (void*)leaf->ch(pos).value;
                std::cout << "  Key " << key << " -> stored NVM pointer: " << stored_ptr << std::endl;
                
                char* value_data = (char*)stored_ptr;
                std::cout << "    Value: " << std::string(value_data + 8, 8) << std::endl;
            }
        }
    }
    
    void analyzeFragmentationAndBookkeeping() {
        std::cout << "\n=== FRAGMENTATION AND BOOKKEEPING ANALYSIS ===" << std::endl;
        
        std::cout << "\n1. CURRENT APPROACH (Fixed-size, contiguous):" << std::endl;
        std::cout << "   Storage: char array[N * 16]" << std::endl;
        std::cout << "   Allocation: array[i * 16] for record i" << std::endl;
        std::cout << "   Fragmentation: NONE (all values same size)" << std::endl;
        std::cout << "   Bookkeeping: Simple index calculation" << std::endl;
        std::cout << "   Pros: Simple, no fragmentation, fast allocation" << std::endl;
        std::cout << "   Cons: Fixed size only, wastes space for small values" << std::endl;
        
        std::cout << "\n2. VARIABLE-SIZE VALUES (Real-world scenario):" << std::endl;
        std::cout << "   Storage: Need dynamic allocator in NVM" << std::endl;
        std::cout << "   Allocation: malloc-like interface" << std::endl;
        std::cout << "   Fragmentation: MAJOR ISSUE!" << std::endl;
        std::cout << "   Bookkeeping: Free lists, size headers, coalescing" << std::endl;
        
        std::cout << "\n3. FRAGMENTATION SCENARIOS:" << std::endl;
        std::cout << "   Insert: 100-byte value -> allocate 100 bytes" << std::endl;
        std::cout << "   Insert: 50-byte value  -> allocate 50 bytes" << std::endl;
        std::cout << "   Delete: 100-byte value -> 100-byte hole" << std::endl;
        std::cout << "   Insert: 75-byte value  -> doesn't fit in 50-byte hole" << std::endl;
        std::cout << "   Result: External fragmentation!" << std::endl;
        
        std::cout << "\n4. BOOKKEEPING REQUIREMENTS:" << std::endl;
        std::cout << "   a) Free list management:" << std::endl;
        std::cout << "      - Track free blocks by size" << std::endl;
        std::cout << "      - Coalesce adjacent free blocks" << std::endl;
        std::cout << "      - Split large blocks for small allocations" << std::endl;
        std::cout << "   b) Allocation metadata:" << std::endl;
        std::cout << "      - Size headers before each value" << std::endl;
        std::cout << "      - Magic numbers for corruption detection" << std::endl;
        std::cout << "      - Alignment requirements" << std::endl;
        std::cout << "   c) Persistence concerns:" << std::endl;
        std::cout << "      - Atomic updates to free lists" << std::endl;
        std::cout << "      - Recovery after crashes" << std::endl;
        std::cout << "      - Consistency of allocator state" << std::endl;
        
        std::cout << "\n5. ALTERNATIVE SOLUTIONS:" << std::endl;
        std::cout << "   a) Slab allocator: Fixed-size pools (16B, 32B, 64B, etc.)" << std::endl;
        std::cout << "   b) Log-structured: Append-only, garbage collection" << std::endl;
        std::cout << "   c) Page-based: Allocate 4KB pages, pack values inside" << std::endl;
        std::cout << "   d) External storage: Use existing KV store (RocksDB, etc.)" << std::endl;
    }
    
    void demonstrateSerializationConcerns() {
        std::cout << "\n=== SERIALIZATION CONCERNS ===" << std::endl;
        
        std::cout << "\n1. CURRENT APPROACH (Binary data):" << std::endl;
        std::cout << "   Format: Raw binary data (memcpy)" << std::endl;
        std::cout << "   Pros: Fast, compact, no parsing" << std::endl;
        std::cout << "   Cons: Not human-readable, endianness issues" << std::endl;
        
        std::cout << "\n2. STRING SERIALIZATION:" << std::endl;
        std::cout << "   Format: JSON, XML, or custom text format" << std::endl;
        std::cout << "   Pros: Human-readable, portable, debuggable" << std::endl;
        std::cout << "   Cons: Larger size, parsing overhead" << std::endl;
        
        std::cout << "\n3. BINARY SERIALIZATION:" << std::endl;
        std::cout << "   Format: Protocol Buffers, MessagePack, etc." << std::endl;
        std::cout << "   Pros: Compact, fast, schema evolution" << std::endl;
        std::cout << "   Cons: Complexity, library dependencies" << std::endl;
        
        // Demonstrate different serialization approaches
        key_type key = 12345;
        std::string str_value = "Hello, World!";
        
        std::cout << "\nSerialization examples for key=" << key << ", value=\"" << str_value << "\":" << std::endl;
        
        // Binary approach (current)
        char binary_data[32];
        memset(binary_data, 0, 32);
        memcpy(binary_data, &key, sizeof(key_type));
        strcpy(binary_data + 8, str_value.c_str());
        std::cout << "  Binary: " << binary_data << " (size: " << (8 + str_value.length() + 1) << " bytes)" << std::endl;
        
        // JSON approach
        std::string json_data = "{\"key\":" + std::to_string(key) + ",\"value\":\"" + str_value + "\"}";
        std::cout << "  JSON: " << json_data << " (size: " << json_data.length() << " bytes)" << std::endl;
        
        // Custom format
        std::string custom_data = std::to_string(key) + "|" + str_value;
        std::cout << "  Custom: " << custom_data << " (size: " << custom_data.length() << " bytes)" << std::endl;
    }
    
    void run() {
        initializeTree();
        analyzeValueStorageOptions();
        demonstrateCurrentApproach();
        demonstrateProperNVMStorage();
        analyzeFragmentationAndBookkeeping();
        demonstrateSerializationConcerns();
        
        std::cout << "\n=== FINAL ANSWERS TO YOUR QUESTIONS ===" << std::endl;
        std::cout << "\n1. 'Values stored somewhere else' - WHERE exactly?" << std::endl;
        std::cout << "   CURRENT: DRAM array (not persistent!)" << std::endl;
        std::cout << "   PROPER: NVM allocator (persistent)" << std::endl;
        
        std::cout << "\n2. 'What data structure for values?'" << std::endl;
        std::cout << "   CURRENT: Simple contiguous array" << std::endl;
        std::cout << "   PROPER: NVM heap with malloc-like allocator" << std::endl;
        
        std::cout << "\n3. 'Serialization as strings?'" << std::endl;
        std::cout << "   CURRENT: Binary data (memcpy)" << std::endl;
        std::cout << "   OPTIONS: JSON, binary formats, custom protocols" << std::endl;
        
        std::cout << "\n4. 'Fragmentation and bookkeeping?'" << std::endl;
        std::cout << "   CURRENT: No fragmentation (fixed size)" << std::endl;
        std::cout << "   REAL WORLD: Major issue requiring sophisticated allocator" << std::endl;
        
        std::cout << "\n5. 'The tree only contains pointers?'" << std::endl;
        std::cout << "   YES! Tree nodes store 8-byte pointers to external values" << std::endl;
        std::cout << "   Tree structure is independent of value size/type" << std::endl;
    }
};

int main() {
    try {
        ValueStorageDeepDive analyzer;
        analyzer.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Analysis failed: " << e.what() << std::endl;
        return 1;
    }
}