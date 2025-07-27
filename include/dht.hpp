#pragma once

#include "aura.pb.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace aura {

// Represents a single node in the routing table
struct DhtPeer {
    std::string id;
    boost::asio::ip::udp::endpoint endpoint;
    // TODO: Add last response time to evict old nodes
};

// K-bucket, stores up to k "good" nodes
class KBucket {
public:
    explicit KBucket(size_t k = 8) : k_(k) {}
    void add_peer(const DhtPeer& peer);
    const std::vector<DhtPeer>& get_peers() const { return peers_; }
private:
    std::vector<DhtPeer> peers_;
    size_t k_;
};

class RoutingTable {
public:
    RoutingTable(const std::string& self_id);
    void add_peer(const DhtPeer& peer);
    // Finds k closest nodes to the given target_id
    std::vector<DhtPeer> find_closest_peers(const std::string& target_id, size_t count);
    std::string get_self_id() const { return self_id_; }
private:
    std::string self_id_;
    std::vector<KBucket> buckets_;
};

class DhtNode {
public:
    DhtNode(boost::asio::io_context& io_context, unsigned short port, const std::string& self_id);
    void start();
    void bootstrap(const std::string& host, unsigned short port);

    // Finds k closest nodes to target_id and calls the callback
    void find_node(const std::string& target_id, std::function<void(const std::vector<DhtPeer>&)> callback);

    // Announces that we have a file (key = file_hash)
    void store_value(const std::string& key, const PeerInfo& provider);

    // Looks for who has a file (key = file_hash)
    void find_value(const std::string& key, std::function<void(const std::vector<PeerInfo>&)> callback);

private:
    void do_receive();
    void handle_message(const MessageWrapper& msg, const boost::asio::ip::udp::endpoint& sender);
    void send(const MessageWrapper& msg, const boost::asio::ip::udp::endpoint& target);

    boost::asio::io_context& io_context_;
    boost::asio::ip::udp::socket socket_;
    RoutingTable routing_table_;
    std::vector<uint8_t> recv_buffer_;

    // Local storage: key (file hash) -> list of provider peers
    std::unordered_map<std::string, std::vector<PeerInfo>> storage_;
    
    // Pending find_value requests: key (file hash) -> callback
    std::unordered_map<std::string, std::function<void(const std::vector<PeerInfo>&)>> pending_find_value_;
    
    // Pending find_node requests: target_id -> callback
    std::unordered_map<std::string, std::function<void(const std::vector<DhtPeer>&)>> pending_find_node_;
};

} // namespace aura
