#include "../include/tcp_server.hpp"
#include "../include/fix_utils.hpp"
#include "../include/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

class MockBistServer : public TcpServer {
public:
    MockBistServer(int port) : TcpServer(port, "MockBistServer") {}

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
            
            // Validate incoming message structure
            std::string validation_error;
            bool is_valid = fix::validate_message(soh_req, validation_error);
            if (!is_valid) {
                logger::log_warn("MockBistServer: Received invalid FIX message: " + validation_error);
                logger::log_warn("Malformed payload: " + fix::to_pipe(req));
            }

            // MsgType check (New Order Single = D)
            std::string msg_type = fix::get_field(soh_req, 35);
            if (msg_type == "D") {
                std::string cl_ord_id = fix::get_field(soh_req, 11);
                if (cl_ord_id.empty()) cl_ord_id = "UNKNOWN";

                std::string symbol = fix::get_field(soh_req, 55);
                std::string side = fix::get_field(soh_req, 54);
                std::string qty = fix::get_field(soh_req, 38);
                std::string price = fix::get_field(soh_req, 44);

                logger::log_info("MockBistServer: Processing NewOrderSingle for " + symbol + 
                                 ", Qty=" + qty + ", Price=" + price + " (ClOrdID:" + cl_ord_id + ")");

                // Generate execution credentials
                static uint64_t exec_counter = 1000;
                std::string exec_id = "EXE" + std::to_string(++exec_counter);
                std::string order_id = "ORD" + std::to_string(exec_counter);

                // Create ExecutionReport (35=8) fields
                std::vector<std::pair<int, std::string>> fields = {
                    {35, "8"},            // MsgType: ExecutionReport
                    {11, cl_ord_id},      // ClOrdID
                    {37, order_id},       // OrderID
                    {17, exec_id},        // ExecID
                    {150, "2"},           // ExecType: Filled
                    {39, "2"},            // OrdStatus: Filled
                    {55, symbol},         // Symbol
                    {54, side},           // Side
                    {38, qty},            // OrderQty
                    {44, price},          // Price
                    {151, "0"},           // LeavesQty
                    {14, qty}             // CumQty
                };

                std::string exec_rep = fix::build_message(fields);
                
                logger::log_info("MockBistServer: Order filled. Transmitting ExecutionReport (ExecID:" + exec_id + ")");
                write(client_fd, exec_rep.c_str(), exec_rep.length());
            }
        }
        close(client_fd);
    }
};

int main() {
    logger::log_info("Starting Mock BIST Server...");
    try {
        MockBistServer server(5003);
        server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting Mock BIST Server: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
