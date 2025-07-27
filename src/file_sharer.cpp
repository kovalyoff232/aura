#include "file_sharer.hpp"
#include "node.hpp"
#include "aura.pb.h"
#include <fstream>
#include <openssl/evp.h>
#include <memory>

// Helper for managing EVP_MD_CTX context
using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

// Utility function for convenient SHA1 hash calculation
std::vector<uint8_t> calculate_sha1(const char* data, size_t len) {
    EVP_MD_CTX_ptr mdctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    const EVP_MD* md = EVP_get_digestbyname("SHA1");

    EVP_DigestInit_ex(mdctx.get(), md, NULL);
    EVP_DigestUpdate(mdctx.get(), data, len);
    
    std::vector<uint8_t> hash(EVP_MAX_MD_SIZE);
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx.get(), hash.data(), &hash_len);
    hash.resize(hash_len);

    return hash;
}


#include <iostream>

namespace aura {

bool FileSharer::share_file(const std::string& file_path, Node& node) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    FileInfo file_info;
    file_info.file_path = file_path;

    file.seekg(0, std::ios::end);
    file_info.file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Calculate the hash of the entire file
    EVP_MD_CTX_ptr mdctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    const EVP_MD* md = EVP_get_digestbyname("SHA1");
    EVP_DigestInit_ex(mdctx.get(), md, NULL);

    char buffer[CHUNK_SIZE];
    while (file) {
        file.read(buffer, CHUNK_SIZE);
        std::streamsize count = file.gcount();
        if (count > 0) {
            // Hash of the chunk
            file_info.chunk_hashes.push_back(calculate_sha1(buffer, count));
            // Update the total hash
            EVP_DigestUpdate(mdctx.get(), buffer, count);
        }
    }

    file_info.file_hash.resize(EVP_MAX_MD_SIZE);
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx.get(), file_info.file_hash.data(), &hash_len);
    file_info.file_hash.resize(hash_len);

    std::string file_hash_str(file_info.file_hash.begin(), file_info.file_hash.end());
    node.available_files_[file_hash_str] = file_info;

    // Save metadata to a .aura file
    aura::Metadata metadata;
    metadata.set_file_hash(file_info.file_hash.data(), file_info.file_hash.size());
    metadata.set_file_size(file_info.file_size);
    metadata.set_chunk_size(CHUNK_SIZE);
    for (const auto& hash : file_info.chunk_hashes) {
        metadata.add_chunk_hashes(hash.data(), hash.size());
    }

    std::string metadata_path = file_path + ".aura";
    std::ofstream metadata_file(metadata_path, std::ios::binary);
    if (!metadata.SerializeToOstream(&metadata_file)) {
        std::cerr << "Failed to write metadata file." << std::endl;
        return false;
    }

    return true;
}

FileInfo FileSharer::load_metadata(const std::string& metadata_path) {
    FileInfo file_info;
    aura::Metadata metadata;

    std::ifstream metadata_file(metadata_path, std::ios::binary);
    if (!metadata.ParseFromIstream(&metadata_file)) {
        return {}; // Return an empty object if reading failed
    }

    file_info.file_hash.assign(metadata.file_hash().begin(), metadata.file_hash().end());
    file_info.file_size = metadata.file_size();
    for (const auto& hash : metadata.chunk_hashes()) {
        file_info.chunk_hashes.emplace_back(hash.begin(), hash.end());
    }

    return file_info;
}

std::vector<uint8_t> FileSharer::get_chunk(const FileInfo& file_info, uint32_t chunk_index) {
    std::ifstream file(file_info.file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FileSharer] ERROR: Could not open file for reading: " << file_info.file_path << std::endl;
        return {};
    }

    file.seekg(chunk_index * CHUNK_SIZE);
    std::vector<uint8_t> data(CHUNK_SIZE);
    file.read(reinterpret_cast<char*>(data.data()), CHUNK_SIZE);
    data.resize(file.gcount());

    std::cout << "[FileSharer] Read chunk " << chunk_index << " from " << file_info.file_path << ", size: " << data.size() << std::endl;
    return data;
}

void FileSharer::save_chunk(const FileInfo& file_info, uint32_t chunk_index, const std::vector<uint8_t>& data) {
    std::fstream file(file_info.file_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cout << "[FileSharer] File not found, creating new one: " << file_info.file_path << std::endl;
        std::ofstream new_file(file_info.file_path, std::ios::binary);
        new_file.close();
        file.open(file_info.file_path, std::ios::binary | std::ios::in | std::ios::out);
    }

    file.seekp(chunk_index * CHUNK_SIZE);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    std::cout << "[FileSharer] Wrote chunk " << chunk_index << " to " << file_info.file_path << ", size: " << data.size() << std::endl;
}

}
