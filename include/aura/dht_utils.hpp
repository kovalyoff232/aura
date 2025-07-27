#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace aura {
namespace dht {

// --- Functions for working with IDs and hashes ---
std::string to_hex(const std::string& s);
std::string from_hex(const std::string& hex_s);

// Calculates the XOR distance between two IDs
inline std::vector<uint8_t> xor_distance(const std::string& id1, const std::string& id2) {
    std::vector<uint8_t> dist;
    dist.reserve(id1.length());
    for (size_t i = 0; i < id1.length(); ++i) {
        dist.push_back(static_cast<uint8_t>(id1[i]) ^ static_cast<uint8_t>(id2[i]));
    }
    return dist;
}

// Finds the index of the first set bit, which determines the bucket number
inline int get_bucket_index(const std::vector<uint8_t>& distance) {
    for (size_t i = 0; i < distance.size(); ++i) {
        if (distance[i] != 0) {
            // clz = count leading zeros
#ifdef _MSC_VER
            unsigned long index;
            _BitScanReverse(&index, distance[i]);
            return i * 8 + (7 - index);
#else
            return i * 8 + __builtin_clz(distance[i]) - (sizeof(unsigned int) - 1) * 8;
#endif
        }
    }
    return -1; // -1 if the distance is 0 (it's us)
}

} // namespace dht
} // namespace aura
