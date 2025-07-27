#pragma once

#include "dht.hpp"
#include "file_sharer.hpp"
#include "session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace aura {

namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class Node {
public:
    Node(boost::asio::io_context& io_context, short tcp_port, short udp_port);

    void listen(short port);
    void connect(const std::string& host, const std::string& port);
    
    // File management
    void announce_file(const std::string& file_path);
    void download_file(const std::string& file_hash_hex);

    // --- Getters ---
    const std::string& get_peer_id() const { return peer_id_; }
    FileSharer& get_file_sharer() { return file_sharer_; }
    DhtNode* get_dht_node() { return dht_node_.get(); }
    ssl::context& get_ssl_context() { return ssl_context_; }

    // Map <file hash, file info>
    std::unordered_map<std::string, FileInfo> available_files_;

private:
    void do_accept();
    void generate_id();
    void remove_session(std::shared_ptr<Session> session);

    boost::asio::io_context& io_context_;
    ssl::context ssl_context_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::string peer_id_; // 160-bit node ID (SHA-1)
    short self_tcp_port_; // Our TCP port to announce in the DHT
    
    // Using a set to store active sessions.
    std::unordered_set<std::shared_ptr<Session>> sessions_;

    FileSharer file_sharer_;
    std::unique_ptr<DhtNode> dht_node_;

    friend class Session; // Give Session access to Node's private methods
};

} // namespace aura
