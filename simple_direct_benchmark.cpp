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

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class SimpleLBTreeBenchmark {
private:
    const size_t NUM_RECORDS;
    std::vector<uint64_t> data;
    
    struct BenchmarkResults {
        double bulkload_time_ms;
        double search_time_ms;
        size_t records_processed;
    } results;

public:
    SimpleLBTreeBenchmark(size_t num_records = 1000000) : NUM_RECORDS(num_records) {
        std::cout << "Initializing Simple LBTree Benchmark with " << NUM_RECORDS << " records..." << std::endl;
    }
    
    void generateData() {
        std::cout << "\nGenerating " << NUM_RECORDS << " random uint64_t records using your macro..." << std::endl;
        
        // Use your specified macro to generate random data
        GENERATE_RANDOM_NUMBER_ARRAY(1, NUM_RECORDS + 1, data);
        
        std::cout << "Data generation completed!" << std::endl;
        std::cout << "Sample data (first 10): ";
        for (int i = 0; i < 10 && i < data.size(); i++) {
            std::cout << data[i] << " ";
        }
        std::cout << std::endl;
    }
    
    void saveDataToFile(const std::string& filename, bool sorted = false) {
        std::vector<uint64_t> output_data = data;
        if (sorted) {
            std::sort(output_data.begin(), output_data.end());
        }
        
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(output_data.data()), 
                      output_data.size() * sizeof(uint64_t));
            file.close();
            std::cout << "Saved " << output_data.size() << " records to " << filename << std::endl;
        } else {
            throw std::runtime_error("Failed to create " + filename);
        }
    }
    
    double runCommand(const std::string& command) {
        std::cout << "Running: " << command << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        int result = std::system(command.c_str());
        auto end = std::chrono::high_resolution_clock::now();
        
        if (result != 0) {
            throw std::runtime_error("Command failed: " + command);
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0; // Convert to milliseconds
    }
    
    void benchmarkBulkload() {
        std::cout << "\n=== BULKLOAD BENCHMARK ===" << std::endl;
        std::cout << "Loading " << NUM_RECORDS << " sorted records into lbtree..." << std::endl;
        
        // Save sorted data for bulkload
        saveDataToFile("bulkload_data.bin", true);
        
        // Clean up any existing NVM file
        std::system("rm -f /tmp/simple_benchmark.nvm");
        
        // Build command
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/simple_benchmark.nvm 100 "
            << "bulkload " << NUM_RECORDS << " bulkload_data.bin 0.8";
        
        results.bulkload_time_ms = runCommand(cmd.str());
        
        std::cout << "Bulkload completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.bulkload_time_ms << " ms" << std::endl;
        
        double throughput = (NUM_RECORDS * 1000.0) / results.bulkload_time_ms;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkSearch() {
        std::cout << "\n=== SEARCH BENCHMARK ===" << std::endl;
        std::cout << "Searching " << NUM_RECORDS << " records in lbtree..." << std::endl;
        
        // Save random data for search
        saveDataToFile("search_data.bin", false);
        
        // Build command
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/simple_benchmark.nvm 100 "
            << "lookup " << NUM_RECORDS << " search_data.bin";
        
        results.search_time_ms = runCommand(cmd.str());
        
        std::cout << "Search completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.search_time_ms << " ms" << std::endl;
        
        double throughput = (NUM_RECORDS * 1000.0) / results.search_time_ms;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    }
    
    void verifyTree() {
        std::cout << "\n=== TREE VERIFICATION ===" << std::endl;
        std::cout << "Checking tree integrity..." << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/simple_benchmark.nvm 100 check_tree";
        
        try {
            runCommand(cmd.str());
            std::cout << "✓ Tree integrity verified!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ Tree verification failed: " << e.what() << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "                BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << "Records: " << NUM_RECORDS << " uint64_t values" << std::endl;
        std::cout << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        std::cout << std::left << std::setw(15) << "Operation" 
                  << std::setw(15) << "Time (ms)" 
                  << std::setw(20) << "Throughput (ops/sec)" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        double bulkload_throughput = (NUM_RECORDS * 1000.0) / results.bulkload_time_ms;
        std::cout << std::left << std::setw(15) << "BULKLOAD" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.bulkload_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << bulkload_throughput << std::endl;
                  
        double search_throughput = (NUM_RECORDS * 1000.0) / results.search_time_ms;
        std::cout << std::left << std::setw(15) << "SEARCH" 
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.search_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << search_throughput << std::endl;
        
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void saveResults(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Simple LBTree Benchmark Results\n";
            file << "================================\n";
            file << "Records: " << NUM_RECORDS << " uint64_t values\n";
            file << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro\n\n";
            
            double bulkload_throughput = (NUM_RECORDS * 1000.0) / results.bulkload_time_ms;
            double search_throughput = (NUM_RECORDS * 1000.0) / results.search_time_ms;
            
            file << "BULKLOAD - Time: " << results.bulkload_time_ms << " ms, ";
            file << "Throughput: " << bulkload_throughput << " ops/sec\n";
            
            file << "SEARCH - Time: " << results.search_time_ms << " ms, ";
            file << "Throughput: " << search_throughput << " ops/sec\n";
            
            file.close();
            std::cout << "\nResults saved to " << filename << std::endl;
        }
    }
    
    void runBenchmark() {
        std::cout << "Starting Simple LBTree Benchmark..." << std::endl;
        
        // Check if lbtree executable exists
        if (std::system("test -f ./lbtree") != 0) {
            std::cout << "Building lbtree..." << std::endl;
            if (std::system("make clean && make lbtree") != 0) {
                throw std::runtime_error("Failed to build lbtree");
            }
        }
        
        try {
            // Generate test data
            generateData();
            
            // Run benchmarks
            benchmarkBulkload();
            benchmarkSearch();
            verifyTree();
            
            // Print summary and save results
            printSummary();
            saveResults("simple_lbtree_benchmark_results.txt");
            
            std::cout << "\n🎉 Simple benchmark completed successfully!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Benchmark failed: " << e.what() << std::endl;
            throw;
        }
        
        // Cleanup
        std::cout << "\nCleaning up temporary files..." << std::endl;
        std::system("rm -f bulkload_data.bin search_data.bin");
        std::cout << "Cleanup completed." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Allow command line argument for number of records
        size_t num_records = 1000000;  // Default 1M
        if (argc > 1) {
            num_records = std::stoull(argv[1]);
        }
        
        std::cout << "=========================================" << std::endl;
        std::cout << "    Simple LBTree Performance Benchmark" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        SimpleLBTreeBenchmark benchmark(num_records);
        benchmark.runBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Benchmark failed with unknown exception" << std::endl;
        return 1;
    }
}