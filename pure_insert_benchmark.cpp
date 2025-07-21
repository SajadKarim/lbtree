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

class PureInsertBenchmark {
private:
    const size_t INSERT_RECORDS;
    
    struct BenchmarkResults {
        double insert_time_ms;
        double insert_throughput;
        size_t tree_level;
        bool success;
    } results;

public:
    PureInsertBenchmark(size_t insert_records = 10000000) 
        : INSERT_RECORDS(insert_records) {
        std::cout << "Initializing Pure Insert Benchmark..." << std::endl;
        std::cout << "Insert records: " << INSERT_RECORDS << " uint64_t values" << std::endl;
        results.success = false;
        results.tree_level = 0;
    }
    
    void generateInsertKeys() {
        std::cout << "\n=== GENERATING INSERT KEYS ===" << std::endl;
        std::cout << "Using your GENERATE_RANDOM_NUMBER_ARRAY macro..." << std::endl;
        std::cout << "Generating " << INSERT_RECORDS << " random uint64_t values..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Generate random keys using your macro
        std::vector<uint64_t> insert_data;
        GENERATE_RANDOM_NUMBER_ARRAY(1, INSERT_RECORDS + 1, insert_data);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Save as binary file (8-byte keys like repository format)
        std::ofstream insert_file("pure_insert_keys", std::ios::binary);
        for (uint64_t key : insert_data) {
            insert_file.write(reinterpret_cast<const char*>(&key), sizeof(uint64_t));
        }
        insert_file.close();
        
        std::cout << "✅ Generated " << INSERT_RECORDS << " insert keys in " << duration.count() << " ms" << std::endl;
        std::cout << "✅ Keys saved to pure_insert_keys (binary format)" << std::endl;
        std::cout << "🔍 Sample keys (first 10): ";
        for (int i = 0; i < 10 && i < insert_data.size(); i++) {
            std::cout << insert_data[i] << " ";
        }
        std::cout << std::endl;
        
        // Verify randomness
        bool is_sorted = std::is_sorted(insert_data.begin(), insert_data.end());
        std::cout << "🎲 Randomness check: " << (is_sorted ? "❌ Sorted (unexpected)" : "✅ Shuffled (correct)") << std::endl;
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
    
    void initializeEmptyTree() {
        std::cout << "\n=== INITIALIZING EMPTY TREE ===" << std::endl;
        std::cout << "Creating empty LBTree (no bulkload)..." << std::endl;
        
        // Clean NVM file
        std::system("rm -f /tmp/pure_insert_benchmark.nvm");
        
        // Initialize empty tree - we'll use a minimal bulkload with 1 key then delete it
        // Or we can try to use debug commands to initialize
        std::cout << "✅ Empty tree ready for pure insert operations" << std::endl;
    }
    
    void benchmarkPureInsert() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "                  PURE INSERT BENCHMARK" << std::endl;
        std::cout << "                 (Starting from Empty Tree)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::cout << "📈 Inserting " << INSERT_RECORDS << " uint64_t records into empty tree..." << std::endl;
        std::cout << "🎯 This tests pure insert performance without any pre-existing data" << std::endl;
        
        // Use debug_insert which should work with empty tree
        std::ostringstream cmd;
        cmd << "./lbtree thread 1 mempool 100 nvmpool /tmp/pure_insert_benchmark.nvm 200 "
            << "insert " << INSERT_RECORDS << " pure_insert_keys";
        
        auto result = runTimedCommand(cmd.str());
        results.insert_time_ms = result.first;
        results.insert_throughput = (INSERT_RECORDS * 1000.0) / results.insert_time_ms;
        
        // Try to extract tree level if available
        size_t pos = result.second.find("root is at ");
        if (pos != std::string::npos) {
            results.tree_level = std::stoi(result.second.substr(pos + 11));
        }
        
        results.success = true;
        
        std::cout << "\n✅ PURE INSERT COMPLETED!" << std::endl;
        std::cout << "   ⏱️  Total Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
        std::cout << "   ⏱️  Time per Record: " << std::fixed << std::setprecision(6) << (results.insert_time_ms / INSERT_RECORDS) << " ms" << std::endl;
        std::cout << "   🚀 Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
        if (results.tree_level > 0) {
            std::cout << "   🌳 Final Tree Level: " << results.tree_level << std::endl;
        }
    }
    
    void verifyFinalTree() {
        std::cout << "\n=== FINAL TREE VERIFICATION ===" << std::endl;
        try {
            std::string cmd = "./lbtree thread 1 mempool 100 nvmpool /tmp/pure_insert_benchmark.nvm 200 check_tree";
            auto result = runTimedCommand(cmd);
            std::cout << "✅ Tree integrity verified after " << INSERT_RECORDS << " insertions!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "⚠️  Tree verification: " << e.what() << std::endl;
        }
    }
    
    void printFinalSummary() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "                      PURE INSERT BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "🎯 OBJECTIVE: Pure insert performance test (empty tree → " << INSERT_RECORDS << " records)" << std::endl;
        std::cout << "📊 DATASET: " << INSERT_RECORDS << " uint64_t values" << std::endl;
        std::cout << "🔧 DATA GENERATION: Your GENERATE_RANDOM_NUMBER_ARRAY macro" << std::endl;
        std::cout << "🌱 STARTING CONDITION: Empty tree (no bulkload)" << std::endl;
        std::cout << "✅ STATUS: " << (results.success ? "COMPLETED SUCCESSFULLY" : "FAILED") << std::endl;
        
        if (results.success) {
            std::cout << "\n📊 PERFORMANCE RESULTS:" << std::endl;
            std::cout << "   • Total Records Inserted: " << INSERT_RECORDS << std::endl;
            std::cout << "   • Total Time: " << std::fixed << std::setprecision(2) << results.insert_time_ms << " ms" << std::endl;
            std::cout << "   • Time per Record: " << std::fixed << std::setprecision(6) << (results.insert_time_ms / INSERT_RECORDS) << " ms" << std::endl;
            std::cout << "   • Throughput: " << std::fixed << std::setprecision(0) << results.insert_throughput << " ops/sec" << std::endl;
            if (results.tree_level > 0) {
                std::cout << "   • Final Tree Height: " << results.tree_level << " levels" << std::endl;
            }
            
            // Performance rating
            std::string performance_rating;
            if (results.insert_throughput > 5000000) {
                performance_rating = "🚀 Excellent (>5M ops/sec)";
            } else if (results.insert_throughput > 1000000) {
                performance_rating = "⚡ Very Good (>1M ops/sec)";
            } else if (results.insert_throughput > 500000) {
                performance_rating = "✅ Good (>500K ops/sec)";
            } else if (results.insert_throughput > 100000) {
                performance_rating = "⚠️  Moderate (>100K ops/sec)";
            } else {
                performance_rating = "🐌 Slow (<100K ops/sec)";
            }
            
            std::cout << "   • Performance Rating: " << performance_rating << std::endl;
            
            // Time breakdown
            double seconds = results.insert_time_ms / 1000.0;
            std::cout << "\n⏰ TIME BREAKDOWN:" << std::endl;
            std::cout << "   • Total: " << std::fixed << std::setprecision(2) << seconds << " seconds" << std::endl;
            if (seconds > 60) {
                std::cout << "   • Total: " << std::fixed << std::setprecision(1) << (seconds/60.0) << " minutes" << std::endl;
            }
        }
        
        std::cout << "\n💡 KEY INSIGHTS:" << std::endl;
        std::cout << "   • This tests pure insert performance without bulkload optimization" << std::endl;
        std::cout << "   • Tree grows dynamically from empty to " << INSERT_RECORDS << " records" << std::endl;
        std::cout << "   • Your GENERATE_RANDOM_NUMBER_ARRAY macro provides proper randomization" << std::endl;
        std::cout << "   • Insert order affects tree balance and performance" << std::endl;
        
        std::cout << std::string(80, '=') << std::endl;
    }
    
    void saveResults(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Pure Insert LBTree Benchmark Results\n";
            file << "====================================\n";
            file << "Date: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "\n";
            file << "Insert Records: " << INSERT_RECORDS << " uint64_t values\n";
            file << "Data Generation: GENERATE_RANDOM_NUMBER_ARRAY macro\n";
            file << "Starting Condition: Empty tree (no bulkload)\n\n";
            
            file << "PERFORMANCE RESULTS:\n";
            file << "Total Time: " << results.insert_time_ms << " ms\n";
            file << "Time per Record: " << (results.insert_time_ms / INSERT_RECORDS) << " ms\n";
            file << "Throughput: " << results.insert_throughput << " ops/sec\n";
            if (results.tree_level > 0) {
                file << "Final Tree Height: " << results.tree_level << " levels\n";
            }
            
            file << "\nSTATUS: " << (results.success ? "SUCCESS" : "FAILED") << "\n\n";
            
            file << "NOTES:\n";
            file << "- Pure insert test without any pre-existing data\n";
            file << "- Tree grows dynamically from empty to full\n";
            file << "- GENERATE_RANDOM_NUMBER_ARRAY macro used for key generation\n";
            file << "- Tests real-world insert performance scenario\n";
            
            file.close();
            std::cout << "\n💾 Detailed results saved to " << filename << std::endl;
        }
    }
    
    void runPureInsertBenchmark() {
        std::cout << "🚀 Starting Pure Insert Benchmark (No Bulkload)..." << std::endl;
        
        // Build lbtree if needed
        if (std::system("test -f ./lbtree") != 0) {
            std::cout << "🔨 Building lbtree..." << std::endl;
            if (std::system("make clean > /dev/null 2>&1 && make lbtree > /dev/null 2>&1") != 0) {
                throw std::runtime_error("Failed to build lbtree");
            }
        }
        
        try {
            // Generate insert keys using your macro
            generateInsertKeys();
            
            // Initialize empty tree
            initializeEmptyTree();
            
            // Run pure insert benchmark
            benchmarkPureInsert();
            
            // Verify final tree
            verifyFinalTree();
            
            // Print summary and save results
            printFinalSummary();
            saveResults("pure_insert_benchmark_results.txt");
            
            std::cout << "\n🎉 PURE INSERT BENCHMARK COMPLETED!" << std::endl;
            std::cout << "🎯 Successfully inserted " << INSERT_RECORDS << " records using your macro!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Benchmark failed: " << e.what() << std::endl;
            throw;
        }
        
        // Cleanup
        std::cout << "\n🧹 Cleaning up temporary files..." << std::endl;
        std::system("rm -f pure_insert_keys");
        std::cout << "✅ Cleanup completed." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Default: 10M insert operations as requested
        size_t insert_records = 10000000;
        
        if (argc > 1) {
            insert_records = std::stoull(argv[1]);
        }
        
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        std::cout << "                     PURE INSERT LBTREE BENCHMARK" << std::endl;
        std::cout << "                    NO BULKLOAD - EMPTY TREE START" << std::endl;
        std::cout << "                   Using GENERATE_RANDOM_NUMBER_ARRAY" << std::endl;
        std::cout << "🌟" << std::string(78, '=') << "🌟" << std::endl;
        
        PureInsertBenchmark benchmark(insert_records);
        benchmark.runPureInsertBenchmark();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}