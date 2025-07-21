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

int main(int argc, char* argv[]) {
    size_t NUM_RECORDS = (argc > 1) ? std::stoull(argv[1]) : 10000000;
    
    std::cout << "Generating " << NUM_RECORDS << " uint64_t records..." << std::endl;
    
    // Generate random data using your specified macro
    std::vector<uint64_t> random_data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, random_data);
    
    // Create sorted data for bulkload
    std::vector<uint64_t> sorted_data = random_data;
    std::sort(sorted_data.begin(), sorted_data.end());
    
    // Save bulkload data (sorted)
    std::ofstream bulkload_file("bulkload_keys.bin", std::ios::binary);
    bulkload_file.write(reinterpret_cast<const char*>(sorted_data.data()), 
                       sorted_data.size() * sizeof(uint64_t));
    bulkload_file.close();
    
    // Save search data (random order for realistic search patterns)
    std::ofstream search_file("search_keys.bin", std::ios::binary);
    search_file.write(reinterpret_cast<const char*>(random_data.data()), 
                     random_data.size() * sizeof(uint64_t));
    search_file.close();
    
    // Create additional search data with some non-existent keys for comprehensive testing
    std::vector<uint64_t> mixed_search_data = random_data;
    // Add some keys that don't exist (beyond our range)
    for (size_t i = 0; i < NUM_RECORDS / 10; ++i) {
        mixed_search_data.push_back(NUM_RECORDS + 1000000 + i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(mixed_search_data.begin(), mixed_search_data.end(), g);
    
    std::ofstream mixed_search_file("mixed_search_keys.bin", std::ios::binary);
    mixed_search_file.write(reinterpret_cast<const char*>(mixed_search_data.data()), 
                           mixed_search_data.size() * sizeof(uint64_t));
    mixed_search_file.close();
    
    std::cout << "Data generation completed!" << std::endl;
    std::cout << "Files created:" << std::endl;
    std::cout << "  bulkload_keys.bin: " << sorted_data.size() << " sorted keys" << std::endl;
    std::cout << "  search_keys.bin: " << random_data.size() << " random keys (all exist)" << std::endl;
    std::cout << "  mixed_search_keys.bin: " << mixed_search_data.size() << " mixed keys (some non-existent)" << std::endl;
    
    return 0;
}
