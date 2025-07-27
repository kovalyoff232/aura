#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace aura {

// Constant for chunk size (256 KB)
const uint32_t CHUNK_SIZE = 256 * 1024;

struct FileInfo {
    std::string file_path;
    std::vector<uint8_t> file_hash;
    uint64_t file_size;
    std::vector<std::vector<uint8_t>> chunk_hashes;
};

class Node; // Forward declaration

class FileSharer {
public:
    // Creates metadata for a file and saves it to a .aura file
    bool share_file(const std::string& file_path, Node& node);

    // Loads metadata from a .aura file
    FileInfo load_metadata(const std::string& metadata_path);

    // Reads a chunk from a file
    std::vector<uint8_t> get_chunk(const FileInfo& file_info, uint32_t chunk_index);

    // Writes a chunk to a file
    void save_chunk(const FileInfo& file_info, uint32_t chunk_index, const std::vector<uint8_t>& data);
};

}
