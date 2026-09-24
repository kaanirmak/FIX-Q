#include <iostream>
#include <string>
#include <vector>
#include <finora/codec.hpp>
#include <finora/pqc_crypto.hpp>
#include <finora/state_guard.hpp>

int main() {
    std::cout << "=== Finora C++ Direct Engine Example ===" << std::endl;

    // 1. Initialize Post-Quantum Crypto Engine
    PqcEngine engine;
    std::cout << "[Engine] Post-Quantum Crypto Primitives initialized." << std::endl;

    // 2. Prepare order message
    std::string order = "8=FIX.4.2\x01" "9=68\x01" "35=D\x01" "49=ALGO_DESK\x01" "56=BORSA\x01"
                        "11=PQC-1001\x01" "55=KCHOL.E\x01" "54=1\x01" "38=1000\x01" "44=210.00\x01" "10=045\x01";

    // 3. Sniff protocol type zero-copy
    auto proto = finora::FinoraCodec::sniff_protocol(reinterpret_cast<const uint8_t*>(order.data()), order.size());
    std::cout << "[Codec] Detected protocol type: " << static_cast<int>(proto) 
              << " (" << finora::protocol_to_string(proto) << ")" << std::endl;

    // 4. Wrap into Finora Wire Format Envelope
    PqcWireBreakdown breakdown;
    std::vector<uint8_t> wire_frame = engine.wrap_packet(order, &breakdown);

    std::cout << "[Wire] Plaintext size: " << breakdown.plaintext_size << " bytes" << std::endl;
    std::cout << "[Wire] Protected wire size: " << breakdown.total_wire_size << " bytes" << std::endl;
    std::cout << "[Wire] ML-KEM-768 Ciphertext: " << breakdown.kem_ciphertext_hex.substr(0, 32) << "..." << std::endl;
    std::cout << "[Wire] ML-DSA-65 Signature:  " << breakdown.mldsa_signature_hex.substr(0, 32) << "..." << std::endl;

    // 5. Unwrap and verify
    std::string decrypted = engine.unwrap_packet(wire_frame);
    std::cout << "[Engine] Successfully unwrapped & verified! Plaintext matches: " 
              << (decrypted == order ? "TRUE" : "FALSE") << std::endl;

    return 0;
}
