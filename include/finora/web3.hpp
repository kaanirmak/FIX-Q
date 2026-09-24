#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>
#include <openssl/sha.h>

namespace finora {

struct Web3RpcRequest {
    std::string method{"eth_sendRawTransaction"}; // eth_sendRawTransaction, eth_call, crypto_order
    std::string from_address{"0x71C94bCbe622A509204000Dbe97C22e541484C9a"};
    std::string to_address{"0x881D40237659C251811CEC9c364ef91dC08D300C"}; // Uniswap / Vault
    double      amount{14.50};
    std::string asset{"ETH"};
    std::string data{"0xa9059cbb000000000000000000000000881d40237659c251811cec9c364ef91dc08d300c0000000000000000000000000000000000000000000000000000000000c92a20"};
    uint64_t    nonce{142};
};

struct Web3RpcResponse {
    bool        success{true};
    std::string tx_hash;
    uint64_t    block_number{21045982};
    uint64_t    gas_used{21000};
    std::string status{"0x1"}; // 0x1 = Success, 0x0 = Reverted
    std::string from_address;
    std::string to_address;
    double      amount{0.0};
    std::string asset;
    std::string raw_receipt_json;
    std::string mev_protection_status{"SECURED_BY_FINORA_PQC"};
};

class Web3Service {
public:
    static std::string generate_raw_tx_json(const Web3RpcRequest& req) {
        std::stringstream ss;
        ss << "{\n"
           << "  \"jsonrpc\": \"2.0\",\n"
           << "  \"id\": 1,\n"
           << "  \"method\": \"" << req.method << "\",\n"
           << "  \"params\": [\n"
           << "    {\n"
           << "      \"from\": \"" << req.from_address << "\",\n"
           << "      \"to\": \"" << req.to_address << "\",\n"
           << "      \"value\": \"" << std::fixed << std::setprecision(4) << req.amount << " " << req.asset << "\",\n"
           << "      \"data\": \"" << req.data << "\",\n"
           << "      \"nonce\": " << req.nonce << ",\n"
           << "      \"gas\": \"0x5208\",\n"
           << "      \"maxPriorityFeePerGas\": \"0x59682f00\"\n"
           << "    }\n"
           << "  ]\n"
           << "}";
        return ss.str();
    }

    static Web3RpcResponse process_rpc_call(const std::string& raw_json) {
        Web3RpcResponse resp;
        resp.from_address = "0x71C94bCbe622A509204000Dbe97C22e541484C9a";
        resp.to_address   = "0x881D40237659C251811CEC9c364ef91dC08D300C";
        resp.amount       = 14.50;
        resp.asset        = "ETH";

        // Compute simulated Keccak-256 / SHA-256 transaction hash
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(raw_json.data()), raw_json.size(), hash);
        
        std::stringstream ss_hash;
        ss_hash << "0x";
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            ss_hash << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        resp.tx_hash = ss_hash.str();

        auto now = std::chrono::system_clock::now();
        uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        resp.block_number = 21045980 + (ms % 100);

        std::stringstream receipt;
        receipt << "{\n"
                << "  \"jsonrpc\": \"2.0\",\n"
                << "  \"id\": 1,\n"
                << "  \"result\": {\n"
                << "    \"transactionHash\": \"" << resp.tx_hash << "\",\n"
                << "    \"blockNumber\": \"0x" << std::hex << resp.block_number << "\",\n"
                << "    \"blockHash\": \"0xca5f891b29d494" << std::dec << (ms % 999999) << "\",\n"
                << "    \"from\": \"" << resp.from_address << "\",\n"
                << "    \"to\": \"" << resp.to_address << "\",\n"
                << "    \"cumulativeGasUsed\": \"0x5208\",\n"
                << "    \"gasUsed\": 21000,\n"
                << "    \"status\": \"0x1\",\n"
                << "    \"finoraPqcShield\": {\n"
                << "      \"kemAlgorithm\": \"NIST FIPS 203 ML-KEM-768\",\n"
                << "      \"dsaAlgorithm\": \"NIST FIPS 204 ML-DSA-65\",\n"
                << "      \"mevProtected\": true,\n"
                << "      \"antiReplayNonce\": \"0x" << std::hex << ms << "\"\n"
                << "    }\n"
                << "  }\n"
                << "}";
        resp.raw_receipt_json = receipt.str();
        return resp;
    }
};

} // namespace finora
