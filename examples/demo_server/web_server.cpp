#include "httplib.h"
#include "finora/fix_utils.hpp"
#include "finora/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <sys/socket.h>
#include "finora/pqc_crypto.hpp"
#include "finora/codec.hpp"
#include "finora/banking.hpp"
#include "finora/web3.hpp"

inline std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); ++c) {
        if (*c == '"') o << "\\\"";
        else if (*c == '\\') o << "\\\\";
        else if (*c == '\b') o << "\\b";
        else if (*c == '\f') o << "\\f";
        else if (*c == '\n') o << "\\n";
        else if (*c == '\r') o << "\\r";
        else if (*c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(*c) <= '\x1f') {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(*c);
        } else {
            o << *c;
        }
    }
    return o.str();
}

std::string exec(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string extract_json_field(const std::string& json, const std::string& key) {
    size_t key_pos = json.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return "";
    size_t colon_pos = json.find(":", key_pos);
    if (colon_pos == std::string::npos) return "";
    
    size_t val_start = colon_pos + 1;
    while (val_start < json.length() && (json[val_start] == ' ' || json[val_start] == '"')) {
        val_start++;
    }
    
    size_t val_end = val_start;
    if ((json[colon_pos + 1] == ' ' && json[colon_pos + 2] == '"') || json[colon_pos + 1] == '"' || (colon_pos + 2 < json.length() && json[colon_pos + 2] == '"')) {
        while (val_end < json.length() && json[val_end] != '"') {
            val_end++;
        }
    } else {
        while (val_end < json.length() && json[val_end] != ',' && json[val_end] != '}' && json[val_end] != '\r' && json[val_end] != '\n') {
            val_end++;
        }
    }
    return json.substr(val_start, val_end - val_start);
}

std::string to_hex(const std::string& input) {
    static const char hex_digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input) {
        output.push_back(hex_digits[(c >> 4) & 0x0F]);
        output.push_back(hex_digits[c & 0x0F]);
    }
    return output;
}

struct LatencyResult {
    double latency;
    std::string response;
};

LatencyResult measure_latency_and_get_response(int port, const std::string& fix_msg) {
    LatencyResult res = {-1.0, ""};
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return res;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        close(sock);
        return res;
    }

    // Set connection and read timeouts (2 seconds)
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return res;
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    send(sock, fix_msg.c_str(), fix_msg.length(), 0);
    
    char buffer[8192] = {0};
    int valread = read(sock, buffer, sizeof(buffer) - 1);
    
    auto end = std::chrono::high_resolution_clock::now();
    close(sock);

    if (valread > 0) {
        buffer[valread] = '\0';
        res.response = std::string(buffer);
        std::chrono::duration<double, std::milli> duration = end - start;
        res.latency = duration.count();
    }
    return res;
}

int main() {
    httplib::Server svr;

    // Serve static files
    std::string static_dir = "./examples/demo_server/static";
    if (!std::ifstream("examples/demo_server/static/index.html").good() && std::ifstream("ui/static/index.html").good()) {
        static_dir = "./ui/static";
    }
    svr.set_mount_point("/", static_dir);

    // Endpoint for live BIST market data
    svr.Get("/api/market_data", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        std::ifstream file("config/market_data.json");
        if (!file.is_open()) {
            // Trigger background feed fetch if file doesn't exist
            if (std::ifstream("examples/mock_exchange/market_data_feed.py").good()) {
                exec("python3 examples/mock_exchange/market_data_feed.py");
            } else {
                exec("python3 src/market_data_feed.py");
            }
            file.open("config/market_data.json");
        }
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "application/json");
        } else {
            res.set_content("{\"error\":\"Market data unavailable\"}", "application/json");
        }
    });

    // Endpoint for manual refresh of BIST prices
    svr.Post("/api/refresh_market", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        logger::log_info("WebServer API: Refreshing live BIST market data from exchange feed.");
        std::string out = exec("python3 src/market_data_feed.py");
        std::ifstream file("config/market_data.json");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "application/json");
        } else {
            res.set_content("{\"status\":\"error\",\"error_message\":\"Failed to refresh feed\"}", "application/json");
        }
    });

    // Endpoint for getting benchmark data
    svr.Get("/api/benchmark_data", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        logger::log_info("WebServer API: Fetching benchmark_results.csv data.");
        
        std::ifstream file("benchmark_results.csv");
        if (!file.is_open()) {
            logger::log_warn("WebServer API: benchmark_results.csv not found.");
            res.set_content("{\"error\":\"No data\"}", "application/json");
            return;
        }

        std::string line;
        std::vector<std::string> labels;
        std::vector<std::string> tls_latencies;
        std::vector<std::string> pqc_latencies;

        double sum_tls = 0;
        double sum_pqc = 0;
        int count = 0;

        std::getline(file, line); // header

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string id, tls, pqc, diff;
            std::getline(ss, id, ',');
            std::getline(ss, tls, ',');
            std::getline(ss, pqc, ',');
            std::getline(ss, diff, ',');

            labels.push_back(id);
            tls_latencies.push_back(tls);
            pqc_latencies.push_back(pqc);

            try {
                sum_tls += std::stod(tls);
                sum_pqc += std::stod(pqc);
                count++;
            } catch (...) {}
        }

        if (count == 0) {
            res.set_content("{\"error\":\"Empty data\"}", "application/json");
            return;
        }

        double avg_tls = sum_tls / count;
        double avg_pqc = sum_pqc / count;
        double avg_diff = avg_pqc - avg_tls;

        std::string json = "{";
        json += "\"labels\":[";
        for (size_t i = 0; i < labels.size(); i++) {
            json += labels[i];
            if (i < labels.size() - 1) json += ",";
        }
        json += "],\"std_latencies\":[";
        for (size_t i = 0; i < tls_latencies.size(); i++) {
            json += tls_latencies[i];
            if (i < tls_latencies.size() - 1) json += ",";
        }
        json += "],\"cpp_latencies\":[";
        for (size_t i = 0; i < pqc_latencies.size(); i++) {
            json += pqc_latencies[i];
            if (i < pqc_latencies.size() - 1) json += ",";
        }
        json += "],\"avg_std\":" + std::to_string(avg_tls);
        json += ",\"avg_cpp\":" + std::to_string(avg_pqc);
        json += ",\"avg_cpp_diff\":" + std::to_string(avg_diff);
        json += "}";

        res.set_content(json, "application/json");
    });

    svr.Get("/api/academic_data", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        logger::log_info("WebServer API: Fetching tail latency and CPU cycle metrics.");
        std::string json = "{\"tail_latency\": ";
        
        std::ifstream f_tail("tail_latency.json");
        if (f_tail.is_open()) {
            std::stringstream buffer;
            buffer << f_tail.rdbuf();
            json += buffer.str();
        } else {
            json += "null";
        }
        
        json += ", \"cpu_cycles\": ";
        
        std::ifstream f_cpu("cpu_cycles.json");
        if (f_cpu.is_open()) {
            std::stringstream buffer;
            buffer << f_cpu.rdbuf();
            json += buffer.str();
        } else {
            json += "null";
        }
        
        json += "}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/upload_orders", [](const httplib::Request& req, httplib::Response& res) {
        logger::log_info("WebServer API: Uploading custom order dataset (" + std::to_string(req.body.length()) + " bytes)");
        
        std::ofstream outfile("uploaded_orders.txt");
        outfile << req.body;
        outfile.close();

        std::string cmd = "./bin/benchmark -f uploaded_orders.txt";
        logger::log_info("WebServer API: Executing benchmark payload: " + cmd);
        exec(cmd.c_str());
        
        res.set_content("{\"status\":\"success\"}", "application/json");
    });

    svr.Post("/api/send_single_order", [](const httplib::Request& req, httplib::Response& res) {
        std::string json_body = req.body;
        
        std::string symbol = extract_json_field(json_body, "symbol");
        std::string side = extract_json_field(json_body, "side");
        std::string qty = extract_json_field(json_body, "qty");
        std::string price = extract_json_field(json_body, "price");

        if (symbol.empty() || side.empty() || qty.empty() || price.empty()) {
            logger::log_warn("WebServer API: Manual order rejected due to missing fields.");
            res.set_content("{\"status\":\"error\",\"error_message\":\"Missing required order fields\"}", "application/json");
            return;
        }

        // Generate unique ClOrdID
        static uint64_t order_seq = 2000;
        std::string cl_ord_id = "ORDM" + std::to_string(++order_seq);

        // Build fully compliant FIX message
        std::vector<std::pair<int, std::string>> fields = {
            {35, "D"}, // New Order Single
            {11, cl_ord_id},
            {21, "1"},
            {55, symbol},
            {54, side},
            {38, qty},
            {40, "2"}, // Limit Order
            {44, price}
        };
        std::string fix_msg = fix::build_message(fields);

        logger::log_info("WebServer API: Transmitting order (ClOrdID:" + cl_ord_id + 
                         ", Sym:" + symbol + ", Side:" + side + ", Qty:" + qty + ", Price:" + price + ")");

        // Measure real latencies to TLS (5007) and PQC (5006) proxies
        LatencyResult tls_res = measure_latency_and_get_response(5007, fix_msg);
        LatencyResult pqc_res = measure_latency_and_get_response(5006, fix_msg);

        double tls_lat = tls_res.latency;
        double pqc_lat = pqc_res.latency;
        std::string bist_response = tls_res.response;
        if (bist_response.empty()) {
            bist_response = pqc_res.response;
        }

        // Check if proxies returned legitimate responses
        if (tls_lat < 0 && pqc_lat < 0) {
            logger::log_error("WebServer API: Both TLS (5007) and PQC (5006) proxies are offline.");
            res.set_content("{\"status\":\"error\",\"error_message\":\"Both TLS and PQC tunnel proxies are unreachable. Please start services with ./start_prod.sh\"}", "application/json");
            return;
        }

        // If one of the tunnels had an error, record real latency as 0 or available
        if (tls_lat < 0) tls_lat = 0.0;
        if (pqc_lat < 0) pqc_lat = 0.0;

        // Hex encode to prevent command line escaping
        std::string hex_msg = to_hex(fix_msg);
        std::string hex_bist = to_hex(bist_response);

        // Execute native C++ genuine PQC tool (OpenSSL 3.6.2 ML-KEM-768 & ML-DSA-65)
        std::string cmd = "./bin/pqc_tool --hex-message " + hex_msg + 
                          " --tls-latency " + std::to_string(tls_lat) + 
                          " --pqc-latency " + std::to_string(pqc_lat) + 
                          " --bist-response-hex " + hex_bist;
        
        logger::log_info("WebServer API: Executing OpenSSL 3.6.2 genuine PQC dissection.");
        std::string pqc_json = exec(cmd.c_str());
        res.set_content(pqc_json, "application/json");
    });

    svr.Post("/api/run_benchmark", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        std::string cmd = "./bin/benchmark -f fix_test_data.txt";
        logger::log_info("WebServer API: Running full benchmark: " + cmd);
        exec(cmd.c_str());
        res.set_content("{\"status\":\"success\"}", "application/json");
    });

    svr.Get("/api/benchmark_report", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        std::ifstream f("report.json");
        if (f.is_open()) {
            std::stringstream buffer;
            buffer << f.rdbuf();
            res.set_content(buffer.str(), "application/json");
        } else {
            res.set_content("{\"status\":\"error\",\"message\":\"Report not generated yet.\"}", "application/json");
        }
    });

    svr.Get("/api/framework/protocols", [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        std::string json = "{\n"
                           "  \"framework\": \"Finora PQC Multi-Protocol Security Gateway\",\n"
                           "  \"version\": \"1.4.0-DEV\",\n"
                           "  \"protocols\": [\n"
                           "    {\"id\": \"fix\", \"name\": \"FIX 4.2 / 4.4 / 5.0SP2\", \"domain\": \"Sermaye Piyasaları & HFT\", \"magic\": \"8=FIX.\", \"type_byte\": \"0x10\", \"status\": \"ACTIVE\"},\n"
                           "    {\"id\": \"ouch\", \"name\": \"Nasdaq OUCH 5.x / ITCH 5.0\", \"domain\": \"Ultra-Düşük Gecikmeli Emir/Piyasa\", \"magic\": \"Type 0x4F\", \"type_byte\": \"0x11\", \"status\": \"ACTIVE\"},\n"
                           "    {\"id\": \"iso20022\", \"name\": \"ISO 20022 SWIFT MX (pacs.008, pain.001)\", \"domain\": \"Çekirdek Bankacılık & TCMB FAST/EFT\", \"magic\": \"<?xml / urn:iso\", \"type_byte\": \"0x20\", \"status\": \"ACTIVE\"},\n"
                           "    {\"id\": \"web3\", \"name\": \"Web3 Ethereum/EVM JSON-RPC\", \"domain\": \"Kripto Varlıklar, Cüzdan & Borsa\", \"magic\": \"{\\\"jsonrpc\\\":\", \"type_byte\": \"0x30\", \"status\": \"ACTIVE\"}\n"
                           "  ]\n"
                           "}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/banking/simulate", [](const httplib::Request& req, httplib::Response& res) {
        static PqcEngine pqc_engine;
        finora::IsoPaymentRequest pay_req;

        std::string mtype = extract_json_field(req.body, "message_type");
        if (!mtype.empty()) pay_req.message_type = mtype;

        std::string s_iban = extract_json_field(req.body, "sender_iban");
        if (!s_iban.empty()) pay_req.sender_iban = s_iban;

        std::string r_iban = extract_json_field(req.body, "receiver_iban");
        if (!r_iban.empty()) pay_req.receiver_iban = r_iban;

        std::string amt_str = extract_json_field(req.body, "amount");
        if (!amt_str.empty()) {
            try { pay_req.amount = std::stod(amt_str); } catch (...) {}
        }

        std::string ccy = extract_json_field(req.body, "currency");
        if (!ccy.empty()) pay_req.currency = ccy;

        std::string rem = extract_json_field(req.body, "remittance_info");
        if (!rem.empty()) pay_req.remittance_info = rem;

        // Generate or extract ISO 20022 XML
        std::string raw_xml = extract_json_field(req.body, "raw_xml");
        if (raw_xml.empty()) {
            if (pay_req.message_type == "pain.001.001.09" || pay_req.message_type == "pain.001") {
                raw_xml = finora::BankingService::generate_pain001_xml(pay_req);
            } else {
                raw_xml = finora::BankingService::generate_pacs008_xml(pay_req);
            }
        }

        // Sniff Protocol via finora-codec (§3.2)
        auto proto = finora::FinoraCodec::sniff_protocol(reinterpret_cast<const uint8_t*>(raw_xml.data()), raw_xml.size());

        // Shield through Finora PQC Gateway
        auto t0 = std::chrono::high_resolution_clock::now();
        PqcWireBreakdown breakdown;
        auto wire_packet = pqc_engine.wrap_packet(raw_xml, &breakdown);
        std::string verified_xml = pqc_engine.unwrap_packet(wire_packet);
        auto t1 = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;

        // Core Banking Execution -> pacs.002
        auto bank_resp = finora::BankingService::process_iso20022_message(verified_xml);

        std::stringstream ss;
        ss << "{\n"
           << "  \"status\": \"success\",\n"
           << "  \"protocol\": \"" << finora::protocol_to_string(proto) << "\",\n"
           << "  \"payload_type\": \"0x20 (ISO 20022 SWIFT)\",\n"
           << "  \"latency_us\": " << std::fixed << std::setprecision(2) << latency_us << ",\n"
           << "  \"tx_id\": \"" << bank_resp.tx_id << "\",\n"
           << "  \"status_code\": \"" << bank_resp.status_code << "\",\n"
           << "  \"raw_xml\": \"" << escape_json(raw_xml) << "\",\n"
           << "  \"pacs002_xml\": \"" << escape_json(bank_resp.raw_pacs002_xml) << "\",\n"
           << "  \"wire_size\": " << wire_packet.size() << ",\n"
           << "  \"kem_ciphertext_hex\": \"" << breakdown.kem_ciphertext_hex << "\",\n"
           << "  \"mldsa_signature_hex\": \"" << breakdown.mldsa_signature_hex << "\",\n"
           << "  \"aes_ciphertext_hex\": \"" << breakdown.aes_ciphertext_hex << "\",\n"
           << "  \"aes_tag_hex\": \"" << breakdown.aes_tag_hex << "\"\n"
           << "}";

        res.set_content(ss.str(), "application/json");
    });

    svr.Post("/api/crypto/simulate", [](const httplib::Request& req, httplib::Response& res) {
        static PqcEngine pqc_engine;
        finora::Web3RpcRequest rpc_req;

        std::string m = extract_json_field(req.body, "method");
        if (!m.empty()) rpc_req.method = m;

        std::string from = extract_json_field(req.body, "from_address");
        if (!from.empty()) rpc_req.from_address = from;

        std::string to = extract_json_field(req.body, "to_address");
        if (!to.empty()) rpc_req.to_address = to;

        std::string amt = extract_json_field(req.body, "amount");
        if (!amt.empty()) {
            try { rpc_req.amount = std::stod(amt); } catch (...) {}
        }

        std::string asset = extract_json_field(req.body, "asset");
        if (!asset.empty()) rpc_req.asset = asset;

        std::string raw_rpc = extract_json_field(req.body, "raw_json");
        if (raw_rpc.empty()) {
            raw_rpc = finora::Web3Service::generate_raw_tx_json(rpc_req);
        }

        // Sniff Protocol via finora-codec (§3.2)
        auto proto = finora::FinoraCodec::sniff_protocol(reinterpret_cast<const uint8_t*>(raw_rpc.data()), raw_rpc.size());

        // Shield through Finora PQC Gateway
        auto t0 = std::chrono::high_resolution_clock::now();
        PqcWireBreakdown breakdown;
        auto wire_packet = pqc_engine.wrap_packet(raw_rpc, &breakdown);
        std::string verified_rpc = pqc_engine.unwrap_packet(wire_packet);
        auto t1 = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;

        // Process Web3 JSON-RPC
        auto web3_resp = finora::Web3Service::process_rpc_call(verified_rpc);

        std::stringstream ss;
        ss << "{\n"
           << "  \"status\": \"success\",\n"
           << "  \"protocol\": \"" << finora::protocol_to_string(proto) << "\",\n"
           << "  \"payload_type\": \"0x30 (Web3 JSON-RPC)\",\n"
           << "  \"latency_us\": " << std::fixed << std::setprecision(2) << latency_us << ",\n"
           << "  \"tx_hash\": \"" << web3_resp.tx_hash << "\",\n"
           << "  \"block_number\": " << web3_resp.block_number << ",\n"
           << "  \"gas_used\": " << web3_resp.gas_used << ",\n"
           << "  \"raw_rpc\": \"" << escape_json(raw_rpc) << "\",\n"
           << "  \"raw_receipt\": \"" << escape_json(web3_resp.raw_receipt_json) << "\",\n"
           << "  \"mev_protection\": \"" << web3_resp.mev_protection_status << "\",\n"
           << "  \"wire_size\": " << wire_packet.size() << ",\n"
           << "  \"kem_ciphertext_hex\": \"" << breakdown.kem_ciphertext_hex << "\",\n"
           << "  \"mldsa_signature_hex\": \"" << breakdown.mldsa_signature_hex << "\",\n"
           << "  \"aes_ciphertext_hex\": \"" << breakdown.aes_ciphertext_hex << "\",\n"
           << "  \"aes_tag_hex\": \"" << breakdown.aes_tag_hex << "\"\n"
           << "}";

        res.set_content(ss.str(), "application/json");
    });

    logger::log_info("WebServer: Finora Terminal served at http://localhost:8080");
    logger::log_info("WebServer: Native C++ Web Server running on port 8080...");
    svr.listen("0.0.0.0", 8080);

    return 0;
}
