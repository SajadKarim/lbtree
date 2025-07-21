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
    const size_t NUM_RECORDS = 1000; // Small test
    
    std::cout << "Generating " << NUM_RECORDS << " test records..." << std::endl;
    
    std::vector<uint64_t> data;
    GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
    
    std::cout << "First 10 generated numbers: ";
    for (int i = 0; i < 10 && i < data.size(); i++) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;
    
    // Write test keys
    std::ofstream test_file("test_keys.bin", std::ios::binary);
    test_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint64_t));
    test_file.close();
    
    std::cout << "Test data written to test_keys.bin (" << data.size() * sizeof(uint64_t) << " bytes)" << std::endl;
    
    return 0;
}