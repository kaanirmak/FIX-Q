#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "finora/fix_utils.hpp"

double measure_real_latency(int port, const std::string& fix_msg) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1.0;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        close(sock);
        return -1.0;
    }

    // Set socket timeout (2 seconds)
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1.0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    send(sock, fix_msg.c_str(), fix_msg.length(), 0);
    
    char buffer[4096] = {0};
    int valread = read(sock, buffer, sizeof(buffer) - 1);
    
    auto end = std::chrono::high_resolution_clock::now();
    close(sock);

    if (valread <= 0) return -1.0;

    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

int main(int argc, char* argv[]) {
    std::string filename = "";

    if (argc >= 3 && std::string(argv[1]) == "-f") {
        filename = argv[2];
    } else {
        std::cerr << "Usage: ./benchmark -f <orders_file.txt>\n";
        return 1;
    }

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return 1;
    }

    std::vector<std::string> orders;
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.empty()) {
            orders.push_back(fix::to_soh(line));
        }
    }
    infile.close();

    std::vector<double> tls_latencies;
    std::vector<double> pqc_latencies;

    std::cout << "Running Real TCP FIX Benchmarks on " << orders.size() << " orders...\n";

    int invalid_count = 0;
    int success_count = 0;

    for (const std::string& order : orders) {
        std::string validation_error;
        if (!fix::validate_message(order, validation_error)) {
            invalid_count++;
            if (invalid_count <= 5) {
                std::cerr << "Benchmark WARNING: Invalid FIX message: " << validation_error << "\n";
            }
        }

        double tls_latency = measure_real_latency(5007, order);
        double pqc_latency = measure_real_latency(5006, order);
        
        // Pure real measurement: Only record if both tunnels answered
        if (tls_latency >= 0.0 && pqc_latency >= 0.0) {
            tls_latencies.push_back(tls_latency);
            pqc_latencies.push_back(pqc_latency);
            success_count++;
        }
    }

    std::cout << "Benchmark Complete: " << success_count << " / " << orders.size() << " real orders measured.\n";

    if (tls_latencies.empty()) {
        std::cerr << "Error: No valid measurements recorded. Please ensure TLS (5007) and PQC (5006) proxies are running.\n";
        return 1;
    }

    // Write raw data to CSV
    std::ofstream file("benchmark_results.csv");
    file << "Order_ID,TLS_1_3_ms,CPP_PQC_Tunnel_ms,CPP_Overhead_ms\n";
    
    std::vector<double> sorted_tls = tls_latencies;
    std::vector<double> sorted_pqc = pqc_latencies;

    for (size_t i = 0; i < tls_latencies.size(); i++) {
        double tls = tls_latencies[i];
        double pqc = pqc_latencies[i];
        file << (i+1) << "," << tls << "," << pqc << "," << (pqc - tls) << "\n";
    }
    file.close();

    // Sort for CDF percentiles
    std::sort(sorted_tls.begin(), sorted_tls.end());
    std::sort(sorted_pqc.begin(), sorted_pqc.end());

    auto get_percentile = [](const std::vector<double>& v, double p) {
        int idx = std::max(0, std::min((int)v.size() - 1, (int)(v.size() * p)));
        return v[idx];
    };

    double tls_p50 = get_percentile(sorted_tls, 0.50);
    double tls_p90 = get_percentile(sorted_tls, 0.90);
    double tls_p99 = get_percentile(sorted_tls, 0.99);
    double tls_p999 = get_percentile(sorted_tls, 0.999);

    double pqc_p50 = get_percentile(sorted_pqc, 0.50);
    double pqc_p90 = get_percentile(sorted_pqc, 0.90);
    double pqc_p99 = get_percentile(sorted_pqc, 0.99);
    double pqc_p999 = get_percentile(sorted_pqc, 0.999);

    // Export Tail Latency JSON
    std::ofstream json_file("tail_latency.json");
    json_file << "{\n"
              << "  \"tls\": {\"p50\": " << tls_p50 << ", \"p90\": " << tls_p90 << ", \"p99\": " << tls_p99 << ", \"p999\": " << tls_p999 << "},\n"
              << "  \"pqc\": {\"p50\": " << pqc_p50 << ", \"p90\": " << pqc_p90 << ", \"p99\": " << pqc_p99 << ", \"p999\": " << pqc_p999 << "}\n"
              << "}\n";
    json_file.close();

    std::cout << "Exported real measurements to benchmark_results.csv and tail_latency.json\n";
    return 0;
}
