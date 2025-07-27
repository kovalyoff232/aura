#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace aura {

// Константа для размера чанка (256 КБ)
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
    // Создает метаданные для файла и сохраняет их в .aura файл
    bool share_file(const std::string& file_path, Node& node);

    // Загружает метаданные из .aura файла
    FileInfo load_metadata(const std::string& metadata_path);

    // Читает чанк из файла
    std::vector<uint8_t> get_chunk(const FileInfo& file_info, uint32_t chunk_index);

    // Записывает чанк в файл
    void save_chunk(const FileInfo& file_info, uint32_t chunk_index, const std::vector<uint8_t>& data);
};

}
