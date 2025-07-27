#include "node.hpp"
#include "session.hpp"
#include "aura/dht_utils.hpp" // Для to_hex, from_hex
#include <iostream>
#include <random>
#include <thread>
#include <openssl/sha.h>
#include <iomanip>

namespace aura {

// Auxiliary functions are moved to dht_utils
namespace dht {
    std::string to_hex(const std::string& s);
    std::string from_hex(const std::string& hex_s);
}

void Node::generate_id() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::string random_data = std::to_string(dis(gen)) + std::to_string(dis(gen));

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(random_data.c_str()), random_data.length(), hash);
    
    peer_id_.assign(reinterpret_cast<const char*>(hash), SHA_DIGEST_LENGTH);

    std::cout << "Generated 160-bit Peer ID: " << dht::to_hex(peer_id_) << std::endl;
}

Node::Node(boost::asio::io_context& io_context, short tcp_port, short udp_port)
    : io_context_(io_context),
      ssl_context_(ssl::context::tlsv12),
      self_tcp_port_(tcp_port) // Save our own TCP port
{
    generate_id();

    ssl_context_.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 | ssl::context::no_sslv3 |
        ssl::context::single_dh_use);
    ssl_context_.set_verify_mode(ssl::verify_none);

    dht_node_ = std::make_unique<DhtNode>(io_context, udp_port, peer_id_);
    dht_node_->start();
}

void Node::listen(short port) {
    acceptor_ = std::make_unique<tcp::acceptor>(io_context_, 
        tcp::endpoint(tcp::v4(), port));
    do_accept();
}

void Node::announce_file(const std::string& file_path) {
    if (file_sharer_.share_file(file_path, *this)) {
        FileInfo file_info = file_sharer_.load_metadata(file_path + ".aura");
        if (file_info.file_hash.empty()) {
            std::cerr << "Failed to create or load metadata for " << file_path << std::endl;
            return;
        }
        file_info.file_path = file_path;
        std::string file_hash_str(file_info.file_hash.begin(), file_info.file_hash.end());
        available_files_[file_hash_str] = file_info;

        std::cout << "Announcing file " << file_path << " with hash " << dht::to_hex(file_hash_str) << std::endl;

        // Announce ourselves as a provider for this file in the DHT
        PeerInfo self_info;
        self_info.set_address("127.0.0.1"); // TODO: Determine our external IP
        self_info.set_port(self_tcp_port_);
        self_info.set_peer_id(peer_id_);
        dht_node_->store_value(file_hash_str, self_info);
    }
}

void Node::download_file(const std::string& file_hash_hex) {
    std::string file_hash = dht::from_hex(file_hash_hex);
    if (file_hash.length() != 20) {
        std::cerr << "Invalid file hash format." << std::endl;
        return;
    }

    std::cout << "Looking for peers with file hash: " << file_hash_hex << std::endl;

    dht_node_->find_value(file_hash, [this](const std::vector<PeerInfo>& providers) {
        if (providers.empty()) {
            std::cout << "No providers found for this file." << std::endl;
            return;
        }

        std::cout << "Found " << providers.size() << " provider(s). Connecting to the first one..." << std::endl;
        const auto& first_provider = providers[0];
        
        // TODO: Download logic should be here.
        // For now, just connect to the first found peer.
        connect(first_provider.address(), std::to_string(first_provider.port()));
    });
}


void Node::connect(const std::string& host, const std::string& port) {
    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host, port);

    // Create a socket and pass it to the session as an rvalue
    auto session = std::make_shared<Session>(tcp::socket(io_context_), *this, Session::Type::CLIENT);
    
    // Capture the session in the lambda to prevent it from being destroyed
    boost::asio::async_connect(session->get_socket().lowest_layer(), endpoints,
        [this, session](const boost::system::error_code& ec, const tcp::endpoint& endpoint) {
            if (!ec) {
                std::cout << "Connection successful. Starting session..." << std::endl;
                sessions_.insert(session);
                session->start();
            } else {
                std::cerr << "Connect error: " << ec.message() << std::endl;
            }
        });
}

void Node::do_accept() {
    acceptor_->async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "Accepted connection. Starting session..." << std::endl;
                auto session = std::make_shared<Session>(std::move(socket), *this, Session::Type::SERVER);
                sessions_.insert(session);
                session->start();
            }
            do_accept();
        });
}

void Node::remove_session(std::shared_ptr<Session> session) {
    if (sessions_.count(session)) {
        sessions_.erase(session);
        // std::cout << "Session closed. Total sessions: " << sessions_.size() << std::endl;
    }
}

}
