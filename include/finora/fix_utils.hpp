#ifndef FIX_UTILS_HPP
#define FIX_UTILS_HPP

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace fix {

// Standard SOH character
const char SOH = '\x01';

// Convert pipes '|' to standard SOH '\x01'
inline std::string to_soh(const std::string& msg) {
    std::string result = msg;
    std::replace(result.begin(), result.end(), '|', SOH);
    return result;
}

// Convert standard SOH '\x01' to pipes '|' for readable printing
inline std::string to_pipe(const std::string& msg) {
    std::string result = msg;
    std::replace(result.begin(), result.end(), SOH, '|');
    return result;
}

// Calculates the checksum of a FIX message string
// The checksum is the sum of all bytes up to, but not including, the check sum field, modulo 256.
inline std::string calculate_checksum(const std::string& msg) {
    size_t checksum_pos = msg.find("10=");
    if (checksum_pos == std::string::npos) {
        // Sum the entire message if 10= is not present
        unsigned int sum = 0;
        for (char c : msg) {
            sum += static_cast<unsigned char>(c);
        }
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << (sum % 256);
        return oss.str();
    }
    
    unsigned int sum = 0;
    for (size_t i = 0; i < checksum_pos; ++i) {
        sum += static_cast<unsigned char>(msg[i]);
    }
    
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << (sum % 256);
    return oss.str();
}

// Build a fully compliant FIX message from fields
inline std::string build_message(const std::vector<std::pair<int, std::string>>& fields) {
    std::string body = "";
    
    // Construct the body (all tags except 8, 9, 10)
    for (const auto& field : fields) {
        if (field.first == 8 || field.first == 9 || field.first == 10) continue;
        body += std::to_string(field.first) + "=" + field.second + SOH;
    }
    
    // Header includes BeginString (tag 8) and BodyLength (tag 9)
    std::string header = "8=FIX.4.4" + std::string(1, SOH) + "9=" + std::to_string(body.length()) + SOH;
    std::string msg_without_checksum = header + body;
    
    // Calculate and append Checksum (tag 10)
    std::string checksum = calculate_checksum(msg_without_checksum);
    return msg_without_checksum + "10=" + checksum + SOH;
}

// Validates a FIX message structure (checks BeginString, BodyLength, and Checksum)
inline bool validate_message(const std::string& raw_msg, std::string& error_msg) {
    std::string msg = raw_msg;
    // Handle pipe-delimited strings gracefully
    if (msg.find('|') != std::string::npos && msg.find(SOH) == std::string::npos) {
        msg = to_soh(msg);
    }
    
    // 1. Basic length checks
    if (msg.length() < 25) {
        error_msg = "Message too short to be valid FIX";
        return false;
    }
    
    // 2. Check BeginString tag 8
    if (msg.compare(0, 10, "8=FIX.4.4\x01") != 0 && msg.compare(0, 10, "8=FIX.4.2\x01") != 0) {
        error_msg = "Invalid or unsupported BeginString (must be FIX.4.4 or FIX.4.2)";
        return false;
    }
    
    // 3. Find BodyLength tag 9
    size_t tag9_pos = msg.find("9=");
    if (tag9_pos == std::string::npos) {
        error_msg = "Missing BodyLength (tag 9)";
        return false;
    }
    
    size_t tag9_val_start = tag9_pos + 2;
    size_t tag9_val_end = msg.find(SOH, tag9_val_start);
    if (tag9_val_end == std::string::npos) {
        error_msg = "Malformed BodyLength (tag 9) field";
        return false;
    }
    
    std::string body_len_str = msg.substr(tag9_val_start, tag9_val_end - tag9_val_start);
    int expected_body_len = 0;
    try {
        expected_body_len = std::stoi(body_len_str);
    } catch (...) {
        error_msg = "BodyLength value is not an integer";
        return false;
    }
    
    // The body length is the count of bytes between the tag 9 SOH (exclusive) and the tag 10 "10=" string (inclusive of its preceding SOH, exclusive of 10=)
    size_t body_start = tag9_val_end + 1;
    size_t tag10_pos = msg.find("10=", body_start);
    if (tag10_pos == std::string::npos) {
        error_msg = "Missing Checksum (tag 10)";
        return false;
    }
    
    int actual_body_len = static_cast<int>(tag10_pos - body_start);
    if (actual_body_len != expected_body_len) {
        error_msg = "BodyLength mismatch: expected " + std::to_string(expected_body_len) + ", got " + std::to_string(actual_body_len);
        return false;
    }
    
    // 4. Validate Checksum
    size_t tag10_val_start = tag10_pos + 3;
    size_t tag10_val_end = msg.find(SOH, tag10_val_start);
    if (tag10_val_end == std::string::npos) {
        // Allow checksum without trailing SOH if it ends the packet
        tag10_val_end = msg.length();
    }
    
    std::string received_checksum = msg.substr(tag10_val_start, 3);
    std::string calculated_checksum = calculate_checksum(msg);
    
    if (received_checksum != calculated_checksum) {
        error_msg = "Checksum mismatch: expected " + calculated_checksum + ", got " + received_checksum;
        return false;
    }
    
    return true;
}

// Extract field value by tag from a FIX message
inline std::string get_field(const std::string& msg, int tag) {
    std::string tag_prefix = std::to_string(tag) + "=";
    size_t pos = msg.find(tag_prefix);
    if (pos == std::string::npos) return "";
    
    // Verify it's preceded by SOH or is the very first tag
    if (pos > 0 && msg[pos - 1] != SOH && msg[pos - 1] != '|') return "";
    
    size_t val_start = pos + tag_prefix.length();
    size_t val_end = msg.find(SOH, val_start);
    if (val_end == std::string::npos) {
        val_end = msg.find('|', val_start);
    }
    if (val_end == std::string::npos) {
        val_end = msg.length();
    }
    
    return msg.substr(val_start, val_end - val_start);
}

} // namespace fix

#endif // FIX_UTILS_HPP
