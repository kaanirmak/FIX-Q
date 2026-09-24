#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace finora {

enum class ProtocolType : uint8_t {
    UNKNOWN     = 0x00,
    FIX_STANDARD= 0x10,
    OUCH_5      = 0x11,
    ITCH_5      = 0x12,
    CME_SBE     = 0x13,
    SWIFT_MX    = 0x20,
    WEB3_RPC    = 0x30
};

inline const char* protocol_to_string(ProtocolType proto) {
    switch (proto) {
        case ProtocolType::FIX_STANDARD: return "FIX 4.2/4.4/5.0SP2";
        case ProtocolType::OUCH_5:       return "Nasdaq OUCH 5.x";
        case ProtocolType::ITCH_5:       return "Nasdaq ITCH 5.0";
        case ProtocolType::CME_SBE:      return "CME SBE (Simple Binary Encoding)";
        case ProtocolType::SWIFT_MX:     return "ISO 20022 SWIFT (MX)";
        case ProtocolType::WEB3_RPC:     return "Web3 JSON-RPC";
        default:                         return "UNKNOWN";
    }
}

class FinoraCodec {
public:
    /**
     * Identifies protocol type via magic byte signature analysis (§3.2).
     */
    static ProtocolType sniff_protocol(const uint8_t* data, size_t len) {
        if (!data || len < 2) return ProtocolType::UNKNOWN;

        // 1. FIX Standard: starts with "8=FIX."
        if (len >= 6 && std::memcmp(data, "8=FIX.", 6) == 0) {
            return ProtocolType::FIX_STANDARD;
        }

        // 2. SWIFT MX (ISO 20022): starts with "<?xml" or contains "urn:iso:std:iso:20022"
        if (len >= 5 && (std::memcmp(data, "<?xml", 5) == 0 || std::memcmp(data, "<Doc", 4) == 0)) {
            return ProtocolType::SWIFT_MX;
        }
        std::string_view sv(reinterpret_cast<const char*>(data), (len > 256 ? 256 : len));
        if (sv.find("urn:iso:std:iso:20022") != std::string_view::npos || sv.find("<AppHdr") != std::string_view::npos) {
            return ProtocolType::SWIFT_MX;
        }

        // 3. Web3 JSON-RPC: starts with {"jsonrpc": or {"method": or {"id":
        if (len >= 10) {
            size_t skip = 0;
            while (skip < len && (data[skip] == ' ' || data[skip] == '\t' || data[skip] == '\r' || data[skip] == '\n')) {
                skip++;
            }
            if (skip < len && data[skip] == '{') {
                std::string_view json_sample(reinterpret_cast<const char*>(data + skip), (len - skip > 64 ? 64 : len - skip));
                if (json_sample.find("\"jsonrpc\"") != std::string_view::npos ||
                    json_sample.find("\"method\"") != std::string_view::npos ||
                    json_sample.find("\"params\"") != std::string_view::npos) {
                    return ProtocolType::WEB3_RPC;
                }
            }
        }

        // 4. ITCH 5.0: Byte 0 is length prefix MSB, or when direct: packet type 'T','S','R','H','A','F','E','C','X','D','U','P','Q','B','I'
        // Often ITCH on mold64/tcp has a 2-byte length prefix where message type is at data[2]
        if (len >= 3) {
            uint16_t itch_len = (static_cast<uint16_t>(data[0]) << 8) | data[1];
            char itch_msg_type = static_cast<char>(data[2]);
            if (itch_len > 0 && itch_len <= 128 && 
                (itch_msg_type == 'T' || itch_msg_type == 'S' || itch_msg_type == 'R' || 
                 itch_msg_type == 'H' || itch_msg_type == 'A' || itch_msg_type == 'F' || 
                 itch_msg_type == 'E' || itch_msg_type == 'C' || itch_msg_type == 'X' || 
                 itch_msg_type == 'D' || itch_msg_type == 'U' || itch_msg_type == 'P')) {
                return ProtocolType::ITCH_5;
            }
        }

        // 5. OUCH 5.x: Byte 0 is message type: 'O' (Enter Order), 'U' (Replace), 'X' (Cancel), 'S' (System), 'A' (Accepted), 'E' (Executed)
        uint8_t ouch_type = data[0];
        if (ouch_type == 'O' || ouch_type == 'U' || ouch_type == 'X' || ouch_type == 'S' ||
            ouch_type == 'A' || ouch_type == 'E' || ouch_type == 'C' || ouch_type == 'D') {
            // Check if length matches known OUCH 5.x message structs
            size_t expected_ouch_len = get_ouch_message_length(ouch_type);
            if (expected_ouch_len > 0 && (len == expected_ouch_len || len >= expected_ouch_len)) {
                return ProtocolType::OUCH_5;
            }
        }

        // 6. CME SBE: Message Header consists of BlockLength (uint16_t), TemplateID (uint16_t), SchemaID (uint16_t), Version (uint16_t)
        if (len >= 8) {
            uint16_t schema_id = *reinterpret_cast<const uint16_t*>(data + 4);
            if (schema_id == 1 || schema_id == 2 || schema_id == 39 || schema_id == 42) {
                return ProtocolType::CME_SBE;
            }
        }

        return ProtocolType::UNKNOWN;
    }

    /**
     * Determines the exact packet frame boundary based on protocol rules (§3.2).
     * Returns:
     *   > 0: Complete packet frame length in bytes
     *   = 0: Incomplete packet (needs more bytes from socket)
     *   < 0: Malformed stream or error
     */
    static ssize_t find_frame_boundary(ProtocolType proto, const uint8_t* data, size_t len) {
        if (!data || len == 0) return 0;

        switch (proto) {
            case ProtocolType::FIX_STANDARD: {
                // Search for "\x0110=" or " 10=" up to checksum ending with \x01
                if (len < 10) return 0;
                std::string_view sv(reinterpret_cast<const char*>(data), len);
                size_t chk_pos = sv.find("\x01""10=");
                if (chk_pos == std::string_view::npos) {
                    chk_pos = sv.find(" 10=");
                }
                if (chk_pos != std::string_view::npos) {
                    // Checksum value is 3 digits + trailing delimiter
                    // e.g. \x0110=123\x01
                    size_t end_delim = sv.find_first_of("\x01\n\r", chk_pos + 4);
                    if (end_delim != std::string_view::npos) {
                        return static_cast<ssize_t>(end_delim + 1);
                    }
                }
                return 0; // Wait for complete checksum field
            }

            case ProtocolType::OUCH_5: {
                uint8_t type = data[0];
                size_t ouch_len = get_ouch_message_length(type);
                if (ouch_len == 0) return -1; // Invalid OUCH type
                if (len >= ouch_len) return static_cast<ssize_t>(ouch_len);
                return 0;
            }

            case ProtocolType::ITCH_5: {
                // 2-byte big-endian length prefix + payload
                if (len < 2) return 0;
                uint16_t payload_len = (static_cast<uint16_t>(data[0]) << 8) | data[1];
                size_t total_frame = 2 + payload_len;
                if (len >= total_frame) return static_cast<ssize_t>(total_frame);
                return 0;
            }

            case ProtocolType::CME_SBE: {
                if (len < 8) return 0;
                uint16_t block_length = *reinterpret_cast<const uint16_t*>(data);
                size_t total_frame = 8 + block_length; // header (8 bytes) + root block
                if (len >= total_frame) return static_cast<ssize_t>(total_frame);
                return 0;
            }

            case ProtocolType::SWIFT_MX: {
                // Search for closing tag </Document> or </AppHdr>
                std::string_view sv(reinterpret_cast<const char*>(data), len);
                size_t doc_end = sv.find("</Document>");
                if (doc_end != std::string_view::npos) {
                    return static_cast<ssize_t>(doc_end + 11);
                }
                size_t hdr_end = sv.find("</AppHdr>");
                if (hdr_end != std::string_view::npos) {
                    return static_cast<ssize_t>(hdr_end + 9);
                }
                return 0;
            }

            case ProtocolType::WEB3_RPC: {
                // Balance JSON curly braces '{' and '}'
                int brace_depth = 0;
                bool in_string = false;
                bool escape_next = false;
                size_t start_idx = 0;

                while (start_idx < len && (data[start_idx] == ' ' || data[start_idx] == '\n' || data[start_idx] == '\r')) {
                    start_idx++;
                }

                if (start_idx >= len || data[start_idx] != '{') return 0;

                for (size_t i = start_idx; i < len; ++i) {
                    char c = static_cast<char>(data[i]);
                    if (escape_next) {
                        escape_next = false;
                        continue;
                    }
                    if (c == '\\') {
                        escape_next = true;
                        continue;
                    }
                    if (c == '"') {
                        in_string = !in_string;
                        continue;
                    }
                    if (!in_string) {
                        if (c == '{') brace_depth++;
                        else if (c == '}') {
                            brace_depth--;
                            if (brace_depth == 0) {
                                return static_cast<ssize_t>(i + 1); // Found complete JSON object
                            }
                        }
                    }
                }
                return 0; // Wait for closing brace
            }

            default:
                return static_cast<ssize_t>(len);
        }
    }

    /**
     * Validates standard FIX checksum (Tag 10).
     */
    static bool validate_fix_checksum(const uint8_t* data, size_t len) {
        if (!data || len < 10) return false;
        std::string_view sv(reinterpret_cast<const char*>(data), len);
        size_t chk_pos = sv.find("\x01""10=");
        if (chk_pos == std::string_view::npos) {
            chk_pos = sv.find(" 10=");
        }
        if (chk_pos == std::string_view::npos) return false;

        uint32_t sum = 0;
        for (size_t i = 0; i <= chk_pos; ++i) {
            sum += data[i];
        }
        uint32_t expected_chk = sum % 256;

        size_t val_start = chk_pos + 4;
        if (val_start + 3 > len) return false;

        char c1 = static_cast<char>(data[val_start]);
        char c2 = static_cast<char>(data[val_start + 1]);
        char c3 = static_cast<char>(data[val_start + 2]);
        if (!isdigit(c1) || !isdigit(c2) || !isdigit(c3)) return false;

        uint32_t actual_chk = (c1 - '0') * 100 + (c2 - '0') * 10 + (c3 - '0');
        return expected_chk == actual_chk;
    }

private:
    static size_t get_ouch_message_length(uint8_t type) {
        switch (type) {
            case 'O': return 48; // Enter Order (inbound)
            case 'U': return 46; // Replace Order
            case 'X': return 19; // Cancel Order
            case 'S': return 10; // System Event
            case 'A': return 66; // Order Accepted
            case 'C': return 40; // Order Executed
            case 'D': return 29; // Order Canceled
            case 'J': return 29; // Order Rejected
            default:  return 0;
        }
    }
};

} // namespace finora
