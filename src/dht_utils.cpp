#include "aura/dht_utils.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace aura {
namespace dht {

std::string to_hex(const std::string& s) {
    std::stringstream hex_stream;
    hex_stream << std::hex << std::setfill('0');
    for (unsigned char c : s) {
        hex_stream << std::setw(2) << static_cast<int>(c);
    }
    return hex_stream.str();
}

std::string from_hex(const std::string& hex_s) {
    if (hex_s.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have an even number of characters.");
    }
    std::string result;
    result.reserve(hex_s.length() / 2);
    for (size_t i = 0; i < hex_s.length(); i += 2) {
        try {
            std::string byte_str = hex_s.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
            result.push_back(byte);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid hex character in string.");
        }
    }
    return result;
}

} // namespace dht
} // namespace aura
