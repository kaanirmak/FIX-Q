#include "finora/tcp_server.hpp"
#include "finora/fix_utils.hpp"
#include "finora/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>

struct MarketQuote {
    std::string symbol;
    double price;
    double bid;
    double ask;
    long volume;
};

class RealBistMatchingEngine : public TcpServer {
private:
    std::mutex book_mutex_;
    std::unordered_map<std::string, MarketQuote> quotes_;
    uint64_t exec_counter_ = 10000;

    void load_market_data() {
        std::ifstream file("config/market_data.json");
        if (!file.is_open()) {
            logger::log_warn("MockBistServer: config/market_data.json not found, using standard BIST anchors.");
            // Standard fallback anchors if file missing
            quotes_["THYAO"] = {"THYAO", 299.00, 298.90, 299.10, 32000000};
            quotes_["GARAN"] = {"GARAN", 133.60, 133.50, 133.70, 23000000};
            quotes_["ASELS"] = {"ASELS", 380.75, 380.50, 381.00, 24000000};
            quotes_["AKBNK"] = {"AKBNK", 73.00, 72.95, 73.05, 89000000};
            quotes_["BIMAS"] = {"BIMAS", 434.00, 433.80, 434.20, 6000000};
            quotes_["EREGL"] = {"EREGL", 38.58, 38.56, 38.60, 97000000};
            quotes_["KCHOL"] = {"KCHOL", 220.80, 220.60, 221.00, 23000000};
            quotes_["TUPRS"] = {"TUPRS", 412.50, 412.20, 412.80, 27000000};
            quotes_["SAHOL"] = {"SAHOL", 91.20, 91.15, 91.25, 28000000};
            quotes_["SISE"]  = {"SISE",  40.24, 40.22, 40.26, 38000000};
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        // Simple JSON extractor for tickers
        std::vector<std::string> symbols = {"THYAO", "GARAN", "ASELS", "AKBNK", "BIMAS", "EREGL", "KCHOL", "TUPRS", "SAHOL", "SISE"};
        for (const auto& sym : symbols) {
            size_t sym_pos = json.find("\"" + sym + "\"");
            if (sym_pos != std::string::npos) {
                auto extract_num = [&](const std::string& key) -> double {
                    size_t kpos = json.find("\"" + key + "\"", sym_pos);
                    if (kpos == std::string::npos || kpos > sym_pos + 600) return 0.0;
                    size_t cpos = json.find(":", kpos);
                    size_t vstart = cpos + 1;
                    while (vstart < json.length() && (json[vstart] == ' ' || json[vstart] == '"')) vstart++;
                    size_t vend = vstart;
                    while (vend < json.length() && json[vend] != ',' && json[vend] != '}' && json[vend] != '\n' && json[vend] != '\r') vend++;
                    try {
                        return std::stod(json.substr(vstart, vend - vstart));
                    } catch (...) {
                        return 0.0;
                    }
                };

                double price = extract_num("price");
                double bid = extract_num("bid");
                double ask = extract_num("ask");
                double vol = extract_num("volume");

                if (price > 0.0) {
                    quotes_[sym] = {sym, price, bid > 0 ? bid : price - 0.05, ask > 0 ? ask : price + 0.05, (long)vol};
                }
            }
        }
        logger::log_info("MockBistServer: Loaded " + std::to_string(quotes_.size()) + " live ticker anchors from config/market_data.json");
    }

public:
    RealBistMatchingEngine(int port) : TcpServer(port, "MockBistServer") {
        load_market_data();
    }

    void handle_client(int client_fd) override {
        logger::log_info("MockBistServer: Client connection accepted.");
        char buffer[4096];

        while (true) {
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) {
                logger::log_info("MockBistServer: Client disconnected.");
                break;
            }
            buffer[bytes_read] = '\0';

            std::string req(buffer);
            std::string soh_req = fix::to_soh(req);

            std::string validation_error;
            bool is_valid = fix::validate_message(soh_req, validation_error);
            if (!is_valid) {
                logger::log_warn("MockBistServer: Invalid FIX message: " + validation_error);
            }

            std::string msg_type = fix::get_field(soh_req, 35);
            if (msg_type == "D") { // New Order Single
                std::string cl_ord_id = fix::get_field(soh_req, 11);
                if (cl_ord_id.empty()) cl_ord_id = "UNKNOWN";

                std::string symbol = fix::get_field(soh_req, 55);
                std::string side = fix::get_field(soh_req, 54); // 1 = Buy, 2 = Sell
                std::string qty_str = fix::get_field(soh_req, 38);
                std::string price_str = fix::get_field(soh_req, 44);

                double order_price = 0.0;
                try {
                    order_price = std::stod(price_str);
                } catch (...) {
                    order_price = 100.0;
                }

                std::lock_guard<std::mutex> lock(book_mutex_);
                
                // Match against live BIST quote
                double exec_price = order_price;
                std::string exec_type = "2"; // 2 = Filled
                std::string ord_status = "2"; // 2 = Filled
                std::string leaves_qty = "0";
                std::string cum_qty = qty_str;

                auto it = quotes_.find(symbol);
                if (it != quotes_.end()) {
                    const auto& q = it->second;
                    if (side == "1") { // Buy order
                        // If order price >= ask, fills at market ask; otherwise limit matched at order price
                        if (order_price >= q.ask) {
                            exec_price = q.ask;
                        } else {
                            exec_price = order_price;
                        }
                    } else { // Sell order
                        if (order_price <= q.bid) {
                            exec_price = q.bid;
                        } else {
                            exec_price = order_price;
                        }
                    }
                }

                std::ostringstream price_stream;
                price_stream << std::fixed << std::setprecision(2) << exec_price;
                std::string formatted_exec_price = price_stream.str();

                uint64_t curr_id = ++exec_counter_;
                std::string exec_id = "BIST-EXE-" + std::to_string(curr_id);
                std::string order_id = "BIST-ORD-" + std::to_string(curr_id);

                logger::log_info("MockBistServer: Order Matched! Symbol=" + symbol + 
                                 " Side=" + (side == "1" ? "BUY" : "SELL") + 
                                 " Qty=" + qty_str + " ExecPrice=" + formatted_exec_price + 
                                 " TL (ClOrdID:" + cl_ord_id + ")");

                // Build standard FIX 4.4 ExecutionReport (35=8)
                std::vector<std::pair<int, std::string>> fields = {
                    {35, "8"},                    // ExecutionReport
                    {11, cl_ord_id},              // ClOrdID
                    {37, order_id},               // OrderID
                    {17, exec_id},                // ExecID
                    {150, exec_type},             // ExecType (2 = Filled)
                    {39, ord_status},             // OrdStatus (2 = Filled)
                    {55, symbol},                 // Symbol
                    {54, side},                   // Side
                    {38, qty_str},                // OrderQty
                    {44, formatted_exec_price},   // Price
                    {31, formatted_exec_price},   // LastPx
                    {32, qty_str},                // LastQty
                    {151, leaves_qty},            // LeavesQty
                    {14, cum_qty},                // CumQty
                    {6, formatted_exec_price}     // AvgPx
                };

                std::string exec_rep = fix::build_message(fields);
                write(client_fd, exec_rep.c_str(), exec_rep.length());
            }
        }
        close(client_fd);
    }
};

int main() {
    signal(SIGPIPE, SIG_IGN);
    logger::log_info("Starting Real BIST FIX Matching Engine (Port 5003)...");
    try {
        RealBistMatchingEngine server(5003);
        server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting Mock BIST Server: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
