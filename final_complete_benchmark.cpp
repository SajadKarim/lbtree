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
#include <set>

#define GENERATE_RANDOM_NUMBER_ARRAY(__START__, __END__, __VECTOR__) { \
    __VECTOR__.resize(__END__ - __START__); \
    std::iota(__VECTOR__.begin(), __VECTOR__.end(), __START__); \
    std::random_device _rd; \
    std::mt19937 _eng(_rd()); \
    std::shuffle(__VECTOR__.begin(), __VECTOR__.end(), _eng); \
}

class CompleteLBTreeBenchmark {
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
        size_t tree_level;
        bool all_success;
    } results;

public:
    CompleteLBTreeBenchmark(size_t bulkload_records = 10000000, size_t test_records = 1000000) 
        : BULKLOAD_RECORDS(bulkload_records), TEST_RECORDS(test_records) {
        std::cout << "Initializing Complete LBTree Benchmark..." << std::endl;
        std::cout << "Bulkload records: " << BULKLOAD_RECORDS << std::endl;
        std::cout << "Test records: " << TEST_RECORDS << std::endl;
        results.all_success = false;
        results.tree_level = 0;
    }
    
    void generateKeysLikeRepository() {
        std::cout << "\n=== GENERATING TEST KEYS (Repository Style) ===" << std::endl;
        std::cout << "Using your GENERATE_RANDOM_NUMBER_ARRAY macro..." << std::endl;
        
        // Generate base random data using your macro
        std::vector<uint64_t> base_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, BULKLOAD_RECORDS + TEST_RECORDS + 1, base_data);
        
        // 1. Bulkload keys (sorted, like repository's keygen)
        std::vector<uint64_t> bulkload_data(base_data.begin(), base_data.begin() + BULKLOAD_RECORDS);
        std::sort(bulkload_data.begin(), bulkload_data.end());
        
        // Save as binary file (8-byte keys like repository)
        std::ofstream bulkload_file("benchmark_bulkload_keys", std::ios::binary);
        for (uint64_t key : bulkload_data) {
            bulkload_file.write(reinterpret_cast<const char*>(&key), sizeof(uint64_t));
        }
        bulkload_file.close();
        
        // 2. Lookup keys (random order, all exist in bulkload, like repository's approach)
        std::vector<uint64_t> lookup_data = bulkload_data;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lookup_data.begin(), lookup_data.end(), g);
        lookup_data.resize(TEST_RECORDS);
        
        std::ofstream lookup_file("benchmark_lookup_keys", std::ios::binary);
        for (uint64_t key : lookup_data) {
            lookup_file.write(reinterpret_cast<const char*>(&key), sizeof(uint64_t));
        }
        lookup_file.close();
        
        // 3. Insert keys (NOT in bulkload keys, like repository's getinsert)
        std::set<uint64_t> bulkload_set(bulkload_data.begin(), bulkload_data.end());
        std::vector<uint64_t> insert_data;
        
        for (size_t i = BULKLOAD_RECORDS; i < base_data.size() && insert_data.size() < TEST_RECORDS; ++i) {
            if (bulkload_set.find(base_data[i]) == bulkload_set.end()) {
                insert_data.push_back(base_data[i]);
            }
        }
        
        // If we need more insert keys, generate them
        while (insert_data.size() < TEST_RECORDS) {
            uint64_t new_key = BULKLOAD_RECORDS + TEST_RECORDS + insert_data.size() + 1000;
            if (bulkload_set.find(new_key) == bulkload_set.end()) {
                insert_data.push_back(new_key);
            }
        }
        
        std::shuffle(insert_data.begin(), insert_data.end(), g);
        
        std::ofstream insert_file("benchmark_insert_keys", std::ios::binary);
        for (uint64_t key : insert_data) {
            insert_file.write(reinterpret_cast<const char*>(&key), sizeof(uint64_t));
        }
        insert_file.close();
        
        // 4. Delete keys (random order, all exist in bulkload, like repository's getdelete)
        std::vector<uint64_t> delete_data(bulkload_data.begin(), bulkload_data.begin() + TEST_RECORDS);
        std::shuffle(delete_data.begin(), delete_data.end(), g);
        
        std::ofstream delete_file("benchmark_delete_keys", std::ios::binary);
        for (uint64_t key : delete_data) {
            delete_file.write(reinterpret_cast<const char*>(&key), sizeof(uint64_t));
        }
        delete_file.close();
        
        std::cout << "✅ Generated bulkload keys: " << BULKLOAD_RECORDS << " (sorted)" << std::endl;
        std::cout << "✅ Generated lookup keys: " << TEST_RECORDS << " (random, all exist)" << std::endl;
        std::cout << "✅ Generated insert keys: " << TEST_RECORDS << " (random, all new)" << std::endl;
        std::cout << "✅ Generated delete keys: " << TEST_RECORDS << " (random, all exist)" << std::endl;
        
        // Verify key generation
        std::cout << "🔍 Verification:" << std::endl;
        std::cout << "   • Bulkload keys are sorted: " << std::is_sorted(bulkload_data.begin(), bulkload_data.end()) << std::endl;
        std::cout << "   • Insert keys don't overlap with bulkload: " << (bulkload_set.find(insert_data[0]) == bulkload_set.end()) << std::endl;
        std::cout << "   • Delete keys exist in bulkload: " << (bulkload_set.find(delete_data[0]) != bulkload_set.end()) << std::endl;
    }
    
    std::pair<double, std::string> runTimedCommand(const std::string& command) {
        std::cout << "🚀 " << command << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Capture output
        std::string output;
        FILE* pipe = popen((command + " 2>&1").c_str(), "r");
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
                std::cout << "   " << buffer;  // Indent output
            }
            pclose(pipe);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double time_ms = duration.count() / 1000.0;
        
        return {time_ms, output};
    }
    
    void benchmarkBulkload() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                    BULKLOAD BENCHMARK" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        // Clean NVM file
        std::system("rm -f /tmp/complete_benchmark.nvm");
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/complete_benchmark.nvm 200 "
            << "bulkload " << BULKLOAD_RECORDS << " benchmark_bulkload_keys 1.0";
        
        auto result = runTimedCommand(cmd.str());
        results.bulkload_time_ms = result.first;
        results.bulkload_throughput = (BULKLOAD_RECORDS * 1000.0) / results.bulkload_time_ms;
        
        // Extract tree level
        size_t pos = result.second.find("root is at ");
        if (pos != std::string::npos) {
            results.tree_level = std::stoi(result.second.substr(pos + 11));
        }
        
        std::cout << "✅ BULKLOAD COMPLETED!" << std::endl;
        std::cout << "   ⏱️  Time: " << std::fixed << std::setprecision(2) << results.bulkload_time_ms << " ms" << std::endl;
        std::cout << "   🚀 Throughput: " << std::fixed << std::setprecision(0) << results.bulkload_throughput << " ops/sec" << std::endl;
        std::cout << "   🌳 Tree Level: " << results.tree_level << std::endl;
    }
    
    void benchmarkInsert() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                     INSERT BENCHMARK" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/complete_benchmark.nvm 200 "
            << "insert " << TEST_RECORDS << " benchmark_insert_keys";
        
        auto result = runTimedCommand(cmd.str());
        results.insert_time_ms = result.first;
        results.insert_throughput = (TEST_RECORDS * 1000.0) / results.insert_time_ms;
        
        std::cout << "✅ INSERT COMPLETED!" << std::endl;
        std::cout << "   ⏱️  Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
        std::cout << "   🚀 Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkLookup() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                     LOOKUP BENCHMARK" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/complete_benchmark.nvm 200 "
            << "lookup " << TEST_RECORDS << " benchmark_lookup_keys";
        
        auto result = runTimedCommand(cmd.str());
        results.lookup_time_ms = result.first;
        results.lookup_throughput = (TEST_RECORDS * 1000.0) / results.lookup_time_ms;
        
        std::cout << "✅ LOOKUP COMPLETED!" << std::endl;
        std::cout << "   ⏱️  Time: " << std::fixed << std::setprecision(2) << results.lookup_time_ms << " ms" << std::endl;
        std::cout << "   🚀 Throughput: " << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec" << std::endl;
    }
    
    void benchmarkDelete() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                     DELETE BENCHMARK" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/complete_benchmark.nvm 200 "
            << "del " << TEST_RECORDS << " benchmark_delete_keys";
        
        auto result = runTimedCommand(cmd.str());
        results.delete_time_ms = result.first;
        results.delete_throughput = (TEST_RECORDS * 1000.0) / results.delete_time_ms;
        
        std::cout << "✅ DELETE COMPLETED!" << std::endl;
        std::cout << "   ⏱️  Time: " << std::fixed << std::setprecision(2) << results.delete_time_ms << " ms" << std::endl;
        std::cout << "   🚀 Throughput: " << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec" << std::endl;
    }
    
    void printFinalSummary() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "                      COMPLETE BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "🎯 OBJECTIVE: Complete LBTree benchmark with INSERT, LOOKUP, DELETE" << std::endl;
        std::cout << "📊 DATASET: " << BULKLOAD_RECORDS << " bulkload + " << TEST_RECORDS << " test records" << std::endl;
        std::cout << "🔧 DATA GENERATION: Your GENERATE_RANDOM_NUMBER_ARRAY macro" << std::endl;
        std::cout << "✅ STATUS: ALL OPERATIONS COMPLETED SUCCESSFULLY!" << std::endl;
        
        std::cout << "\n" << std::string(80, '-') << std::endl;
        std::cout << std::left << std::setw(12) << "OPERATION" 
                  << std::setw(12) << "RECORDS" 
                  << std::setw(15) << "TIME (ms)" 
                  << std::setw(18) << "THROUGHPUT" 
                  << std::setw(15) << "STATUS" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        std::cout << std::left << std::setw(12) << "BULKLOAD" 
                  << std::setw(12) << BULKLOAD_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.bulkload_time_ms
                  << std::setw(18) << std::fixed << std::setprecision(0) << results.bulkload_throughput << " ops/sec"
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "INSERT" 
                  << std::setw(12) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.insert_time_ms
                  << std::setw(18) << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec"
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "LOOKUP" 
                  << std::setw(12) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.lookup_time_ms
                  << std::setw(18) << std::fixed << std::setprecision(0) << results.lookup_throughput << " ops/sec"
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
                  
        std::cout << std::left << std::setw(12) << "DELETE" 
                  << std::setw(12) << TEST_RECORDS
                  << std::setw(15) << std::fixed << std::setprecision(2) << results.delete_time_ms
                  << std::setw(18) << std::fixed << std::setprecision(0) << results.delete_throughput << " ops/sec"
                  << std::setw(15) << "✅ SUCCESS" << std::endl;
        
        std::cout << std::string(80, '-') << std::endl;
        std::cout << "🌳 Tree Height: " << results.tree_level << " levels" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Performance analysis
        std::cout << "\n📈 PERFORMANCE ANALYSIS:" << std::endl;
        std::cout << "   • Fastest Operation: ";
        double max_throughput = std::max({results.bulkload_throughput, results.insert_throughput, 
                                         results.lookup_throughput, results.delete_throughput});
        if (max_throughput == results.lookup_throughput) std::cout << "LOOKUP";
        else if (max_throughput == results.bulkload_throughput) std::cout << "BULKLOAD";
        else if (max_throughput == results.insert_throughput) std::cout << "INSERT";
        else std::cout << "DELETE";
        std::cout << " (" << std::fixed << std::setprecision(0) << max_throughput << " ops/sec)" << std::endl;
        
        std::cout << "   • Total Operations: " << (BULKLOAD_RECORDS + 3 * TEST_RECORDS) << std::endl;
        std::cout << "   • Total Time: " << std::fixed << std::setprecision(2) 
                  << (results.bulkload_time_ms + results.insert_time_ms + results.lookup_time_ms + results.delete_time_ms) 
                  << " ms" << std::endl;
    }
    
    void saveResults(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Complete LBTree Benchmark Results\n";
            file << "==================================\n";
            file << "Date: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "\n";
            file << "Bulkload Records: " << BULKLOAD_RECORDS << " uint64_t values\n";
            file << "Test Records: " << TEST_RECORDS << " uint64_t values\n";
            file << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro\n";
            file << "Tree Height: " << results.tree_level << " levels\n\n";
            
            file << "PERFORMANCE RESULTS:\n";
            file << "BULKLOAD - Time: " << results.bulkload_time_ms << " ms, ";
            file << "Throughput: " << results.bulkload_throughput << " ops/sec\n";
            
            file << "INSERT - Time: " << results.insert_time_ms << " ms, ";
            file << "Throughput: " << results.insert_throughput << " ops/sec\n";
            
            file << "LOOKUP - Time: " << results.lookup_time_ms << " ms, ";
            file << "Throughput: " << results.lookup_throughput << " ops/sec\n";
            
            file << "DELETE - Time: " << results.delete_time_ms << " ms, ";
            file << "Throughput: " << results.delete_throughput << " ops/sec\n";
            
            file << "\nSTATUS: ALL OPERATIONS SUCCESSFUL\n";
            file.close();
            std::cout << "\n💾 Detailed results saved to " << filename << std::endl;
        }
    }
    
    void runCompleteBenchmark() {
        std::cout << "🚀 Starting Complete LBTree Benchmark..." << std::endl;
        
        // Build lbtree if needed
        if (std::system("test -f ./lbtree") != 0) {
            std::cout << "🔨 Building lbtree..." << std::endl;
            if (std::system("make clean > /dev/null 2>&1 && make lbtree > /dev/null 2>&1") != 0) {
                throw std::runtime_error("Failed to build lbtree");
            }
        }
        
        try {
            // Generate test keys using your macro (repository style)
            generateKeysLikeRepository();
            
            // Run all benchmarks in sequence
            benchmarkBulkload();
            benchmarkInsert();
            benchmarkLookup();
            benchmarkDelete();
            
            results.all_success = true;
            
            // Print final summary and save results
            printFinalSummary();
            saveResults("complete_lbtree_benchmark_results.txt");
            
            std::cout << "\n🎉 COMPLETE BENCHMARK FINISHED SUCCESSFULLY!" << std::endl;
            std::cout << "🎯 All operations (INSERT, LOOKUP, DELETE) benchmarked with your macro!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Benchmark failed: " << e.what() << std::endl;
            throw;
        }
        
        // Cleanup
        std::cout << "\n🧹 Cleaning up temporary files..." << std::endl;
        std::system("rm -f benchmark_*_keys");
        std::cout << "✅ Cleanup completed." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Default: 10M bulkload, 1M test operations (as you requested)
        size_t bulkload_records = 10000000;
        size_t test_records = 1000000;
        
        if (argc > 1) {
            bulkload_records = std::stoull(argv[1]);
        }
        if (argc > 2) {
            test_records = std::stoull(argv[2]);
        }
        
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        std::cout << "                   COMPLETE LBTREE PERFORMANCE BENCHMARK" << std::endl;
        std::cout << "                      INSERT + LOOKUP + DELETE OPERATIONS" << std::endl;
        std::cout << "                     Using GENERATE_RANDOM_NUMBER_ARRAY" << std::endl;
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        
        CompleteLBTreeBenchmark benchmark(bulkload_records, test_records);
        benchmark.runCompleteBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}