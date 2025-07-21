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

class ProperLBTreeBenchmark {
private:
    const size_t BULKLOAD_RECORDS;
    const size_t TEST_RECORDS;
    
    struct BenchmarkResults {
        double bulkload_time_ms;
        double insert_time_ms;
        double lookup_time_ms;
        double delete_time_ms;
        double bulkload_throughput;
        double insert_throughput;
        double lookup_throughput;
        double delete_throughput;
        bool all_success;
    } results;

public:
    ProperLBTreeBenchmark(size_t bulkload_records = 1000000, size_t test_records = 100000) 
        : BULKLOAD_RECORDS(bulkload_records), TEST_RECORDS(test_records) {
        std::cout << "Initializing Proper LBTree Benchmark..." << std::endl;
        std::cout << "Bulkload records: " << BULKLOAD_RECORDS << std::endl;
        std::cout << "Test records: " << TEST_RECORDS << std::endl;
        results.all_success = false;
    }
    
    void generateKeys() {
        std::cout << "\n=== GENERATING TEST KEYS ===" << std::endl;
        std::cout << "Using your GENERATE_RANDOM_NUMBER_ARRAY macro..." << std::endl;
        
        // Generate bulkload keys (sorted)
        std::vector<uint64_t> bulkload_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, BULKLOAD_RECORDS + 1, bulkload_data);
        std::sort(bulkload_data.begin(), bulkload_data.end());
        
        std::ofstream bulkload_file("benchmark_bulkload_keys", std::ios::binary);
        bulkload_file.write(reinterpret_cast<const char*>(bulkload_data.data()), 
                           bulkload_data.size() * sizeof(uint64_t));
        bulkload_file.close();
        
        // Generate lookup keys (random order, all exist in bulkload)
        std::vector<uint64_t> lookup_data = bulkload_data;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lookup_data.begin(), lookup_data.end(), g);
        lookup_data.resize(TEST_RECORDS);
        
        std::ofstream lookup_file("benchmark_lookup_keys", std::ios::binary);
        lookup_file.write(reinterpret_cast<const char*>(lookup_data.data()), 
                         lookup_data.size() * sizeof(uint64_t));
        lookup_file.close();
        
        // Generate insert keys (random, NOT in bulkload keys)
        std::vector<uint64_t> insert_data;
        GENERATE_RANDOM_NUMBER_ARRAY(BULKLOAD_RECORDS + 1, BULKLOAD_RECORDS + TEST_RECORDS + 1, insert_data);
        
        std::ofstream insert_file("benchmark_insert_keys", std::ios::binary);
        insert_file.write(reinterpret_cast<const char*>(insert_data.data()), 
                         insert_data.size() * sizeof(uint64_t));
        insert_file.close();
        
        // Generate delete keys (random order, all exist in bulkload)
        std::vector<uint64_t> delete_data(bulkload_data.begin(), bulkload_data.begin() + TEST_RECORDS);
        std::shuffle(delete_data.begin(), delete_data.end(), g);
        
        std::ofstream delete_file("benchmark_delete_keys", std::ios::binary);
        delete_file.write(reinterpret_cast<const char*>(delete_data.data()), 
                         delete_data.size() * sizeof(uint64_t));
        delete_file.close();
        
        std::cout << "✓ Generated bulkload keys: " << BULKLOAD_RECORDS << " (sorted)" << std::endl;
        std::cout << "✓ Generated lookup keys: " << TEST_RECORDS << " (random, all exist)" << std::endl;
        std::cout << "✓ Generated insert keys: " << TEST_RECORDS << " (random, all new)" << std::endl;
        std::cout << "✓ Generated delete keys: " << TEST_RECORDS << " (random, all exist)" << std::endl;
    }
    
    double runTimedCommand(const std::string& command) {
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
        std::cout << "Loading " << BULKLOAD_RECORDS << " records into tree..." << std::endl;
        
        // Clean NVM file
        std::system("rm -f /tmp/proper_benchmark.nvm");
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 200 nvmpool /tmp/proper_benchmark.nvm 200 "
            << "bulkload " << BULKLOAD_RECORDS << " benchmark_bulkload_keys 0.8";
        
        results.bulkload_time_ms = runTimedCommand(cmd.str());
        results.bulkload_throughput = (BULKLOAD_RECORDS * 1000.0) / results.bulkload_time_ms;
        
        std::cout << "✅ Bulkload completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.bulkload_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.bulkload_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkInsert() {
        std::cout << "\n=== INSERT BENCHMARK ===" << std::endl;
        std::cout << "Inserting " << TEST_RECORDS << " new records..." << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 200 nvmpool /tmp/proper_benchmark.nvm 200 "
            << "insert " << TEST_RECORDS << " benchmark_insert_keys";
        
        results.insert_time_ms = runTimedCommand(cmd.str());
        results.insert_throughput = (TEST_RECORDS * 1000.0) / results.insert_time_ms;
        
        std::cout << "✅ Insert completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkLookup() {
        std::cout << "\n=== LOOKUP BENCHMARK ===" << std::endl;
        std::cout << "Looking up " << TEST_RECORDS << " existing records..." << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 200 nvmpool /tmp/proper_benchmark.nvm 200 "
            << "lookup " << TEST_RECORDS << " benchmark_lookup_keys";
        
        results.lookup_time_ms = runTimedCommand(cmd.str());
        results.lookup_throughput = (TEST_RECORDS * 1000.0) / results.lookup_time_ms;
        
        std::cout << "✅ Lookup completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.lookup_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\n=== DELETE BENCHMARK ===" << std::endl;
        std::cout << "Deleting " << TEST_RECORDS << " existing records..." << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 200 nvmpool /tmp/proper_benchmark.nvm 200 "
            << "del " << TEST_RECORDS << " benchmark_delete_keys";
        
        results.delete_time_ms = runTimedCommand(cmd.str());
        results.delete_throughput = (TEST_RECORDS * 1000.0) / results.delete_time_ms;
        
        std::cout << "✅ Delete completed!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec" << std::endl;
    }
    
    void verifyTree() {
        std::cout << "\n=== TREE VERIFICATION ===" << std::endl;
        try {
            std::string cmd = "./lbtree thread 1 mempool 200 nvmpool /tmp/proper_benchmark.nvm 200 check_tree";
            runTimedCommand(cmd);
            std::cout << "✅ Tree integrity verified!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Tree verification failed: " << e.what() << std::endl;
        }
    }
    
    void printSummary() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "                         BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "  • Bulkload Records: " << BULKLOAD_RECORDS << " uint64_t values" << std::endl;
        std::cout << "  • Test Records: " << TEST_RECORDS << " uint64_t values" << std::endl;
        std::cout << "  • Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        std::cout << std::left << std::setw(12) << "Operation" 
                  << std::setw(15) << "Records" 
                  << std::setw(15) << "Time (ms)" 
                  << std::setw(20) << "Throughput (ops/sec)" 
                  << std::setw(15) << "Status" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        std::cout << std::left << std::setw(12) << "BULKLOAD" 
                  << std::setw(15) << BULKLOAD_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.bulkload_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.bulkload_throughput
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "INSERT" 
                  << std::setw(15) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.insert_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.insert_throughput
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "LOOKUP" 
                  << std::setw(15) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.lookup_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.lookup_throughput
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "DELETE" 
                  << std::setw(15) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.delete_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(0) << results.delete_throughput
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
        
        std::cout << std::string(80, '=') << std::endl;
    }
    
    void saveResults(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Proper LBTree Benchmark Results\n";
            file << "================================\n";
            file << "Bulkload Records: " << BULKLOAD_RECORDS << " uint64_t values\n";
            file << "Test Records: " << TEST_RECORDS << " uint64_t values\n";
            file << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro\n\n";
            
            file << "BULKLOAD - Time: " << results.bulkload_time_ms << " ms, ";
            file << "Throughput: " << results.bulkload_throughput << " ops/sec\n";
            
            file << "INSERT - Time: " << results.insert_time_ms << " ms, ";
            file << "Throughput: " << results.insert_throughput << " ops/sec\n";
            
            file << "LOOKUP - Time: " << results.lookup_time_ms << " ms, ";
            file << "Throughput: " << results.lookup_throughput << " ops/sec\n";
            
            file << "DELETE - Time: " << results.delete_time_ms << " ms, ";
            file << "Throughput: " << results.delete_throughput << " ops/sec\n";
            
            file.close();
            std::cout << "\nResults saved to " << filename << std::endl;
        }
    }
    
    void runBenchmark() {
        std::cout << "🚀 Starting Proper LBTree Benchmark (Following README approach)..." << std::endl;
        
        // Build lbtree if needed
        if (std::system("test -f ./lbtree") != 0) {
            std::cout << "Building lbtree..." << std::endl;
            if (std::system("make clean > /dev/null 2>&1 && make lbtree > /dev/null 2>&1") != 0) {
                throw std::runtime_error("Failed to build lbtree");
            }
        }
        
        try {
            // Generate test keys using your macro
            generateKeys();
            
            // Run benchmarks in the proper sequence (as per README)
            benchmarkBulkload();
            verifyTree();
            
            benchmarkInsert();
            verifyTree();
            
            benchmarkLookup();
            verifyTree();
            
            benchmarkDelete();
            verifyTree();
            
            results.all_success = true;
            
            // Print summary and save results
            printSummary();
            saveResults("proper_lbtree_benchmark_results.txt");
            
            std::cout << "\n🎉 All benchmarks completed successfully!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Benchmark failed: " << e.what() << std::endl;
            throw;
        }
        
        // Cleanup
        std::cout << "\nCleaning up temporary files..." << std::endl;
        std::system("rm -f benchmark_*_keys");
        std::cout << "Cleanup completed." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Allow command line arguments for bulkload and test record counts
        size_t bulkload_records = 1000000;  // Default 1M for bulkload
        size_t test_records = 100000;       // Default 100K for insert/lookup/delete
        
        if (argc > 1) {
            bulkload_records = std::stoull(argv[1]);
        }
        if (argc > 2) {
            test_records = std::stoull(argv[2]);
        }
        
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        std::cout << "                    Proper LBTree Performance Benchmark" << std::endl;
        std::cout << "                   Following README.md Approach Exactly" << std::endl;
        std::cout << "                  Using GENERATE_RANDOM_NUMBER_ARRAY" << std::endl;
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        
        ProperLBTreeBenchmark benchmark(bulkload_records, test_records);
        benchmark.runBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "💥 Benchmark failed with unknown exception" << std::endl;
        return 1;
    }
}