#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <cstdint>

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

int main() {
    const size_t NUM_RECORDS = 10000000; // 10M records
    
    std::cout << "Generating " << NUM_RECORDS << " random uint64_t records..." << std::endl;
    
    // Generate shuffled sequence from 1 to NUM_RECORDS
    std::vector<uint64_t> data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
    
    // Write insert keys
    std::ofstream insert_file("insert_keys.bin", std::ios::binary);
    if (!insert_file) {
        std::cerr << "Failed to create insert_keys.bin" << std::endl;
        return 1;
    }
    insert_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    insert_file.close();
    
    // Write search keys (same as insert keys for successful searches)
    std::ofstream search_file("search_keys.bin", std::ios::binary);
    if (!search_file) {
        std::cerr << "Failed to create search_keys.bin" << std::endl;
        return 1;
    }
    search_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    search_file.close();
    
    // Create delete keys (first half of the data for partial deletion)
    std::vector<uint64_t> delete_keys(data.begin(), data.begin() + NUM_RECORDS / 2);
    
    // Shuffle delete keys to avoid sequential deletion
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(delete_keys.begin(), delete_keys.end(), g);
    
    // Write delete keys
    std::ofstream delete_file("delete_keys.bin", std::ios::binary);
    if (!delete_file) {
        std::cerr << "Failed to create delete_keys.bin" << std::endl;
        return 1;
    }
    delete_file.write(reinterpret_cast<const char*>(delete_keys.data()), delete_keys.size() * sizeof(uint64_t));
    delete_file.close();
    
    std::cout << "Data generation completed!" << std::endl;
    std::cout << "Files created:" << std::endl;
    std::cout << "  insert_keys.bin: " << data.size() << " keys (" << (data.size() * sizeof(uint64_t)) / (1024*1024) << " MB)" << std::endl;
    std::cout << "  search_keys.bin: " << data.size() << " keys (" << (data.size() * sizeof(uint64_t)) / (1024*1024) << " MB)" << std::endl;
    std::cout << "  delete_keys.bin: " << delete_keys.size() << " keys (" << (delete_keys.size() * sizeof(uint64_t)) / (1024*1024) << " MB)" << std::endl;
    
    return 0;
}