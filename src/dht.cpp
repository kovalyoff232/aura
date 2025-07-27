#include "dht.hpp"
#include "aura/dht_utils.hpp"
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

namespace aura {

// --- KBucket ---
void KBucket::add_peer(const DhtPeer& peer) {
    auto it = std::find_if(peers_.begin(), peers_.end(), [&](const DhtPeer& p) {
        return p.id == peer.id;
    });

    if (it == peers_.end()) {
        if (peers_.size() < k_) {
            peers_.push_back(peer);
        } else {
            // TODO: PING oldest node
        }
    } else {
        DhtPeer existing_peer = *it;
        peers_.erase(it);
        peers_.push_back(existing_peer);
    }
}

// --- RoutingTable ---
RoutingTable::RoutingTable(const std::string& self_id) : self_id_(self_id) {
    buckets_.resize(160); 
}

void RoutingTable::add_peer(const DhtPeer& peer) {
    if (peer.id == self_id_) {
        return;
    }
    
    auto distance = dht::xor_distance(self_id_, peer.id);
    int bucket_index = dht::get_bucket_index(distance);

    if (bucket_index >= 0 && bucket_index < 160) {
        std::cout << "[RoutingTable] Adding peer " << dht::to_hex(peer.id) << " to bucket " << bucket_index << std::endl;
        buckets_[bucket_index].add_peer(peer);
    }
}

std::vector<DhtPeer> RoutingTable::find_closest_peers(const std::string& target_id, size_t count) {
    std::vector<DhtPeer> all_peers;
    for (const auto& bucket : buckets_) {
        for (const auto& peer : bucket.get_peers()) {
            all_peers.push_back(peer);
        }
    }

    std::sort(all_peers.begin(), all_peers.end(), 
        [&target_id](const DhtPeer& a, const DhtPeer& b) {
            auto dist_a = dht::xor_distance(a.id, target_id);
            auto dist_b = dht::xor_distance(b.id, target_id);
            return dist_a < dist_b;
        }
    );

    if (all_peers.size() > count) {
        all_peers.resize(count);
    }

    return all_peers;
}

// --- DhtNode ---
DhtNode::DhtNode(boost::asio::io_context& io_context, unsigned short port, const std::string& self_id)
    : io_context_(io_context),
      socket_(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)),
      routing_table_(self_id),
      recv_buffer_(4096)
{
    std::cout << "[DHT] Listening on UDP port " << port << std::endl;
}

void DhtNode::start() {
    do_receive();
}

void DhtNode::find_node(const std::string& target_id, std::function<void(const std::vector<DhtPeer>&)> callback) {
    // Это заглушка для итеративного поиска.
    // В реальном Kademlia здесь был бы сложный асинхронный процесс.
    // Пока что мы просто возвращаем k ближайших из нашей таблицы.
    // Этого достаточно, чтобы bootstrap-механизм заработал.
    auto closest = routing_table_.find_closest_peers(target_id, 8);
    boost::asio::post(io_context_, [callback, closest](){
        callback(closest);
    });
}

void DhtNode::store_value(const std::string& key, const PeerInfo& provider) {
    std::cout << "[DHT] Storing value for key " << dht::to_hex(key) << " on the network..." << std::endl;
    
    find_node(key, [this, key, provider](const std::vector<DhtPeer>& closest_peers) {
        std::cout << "[DHT] Found " << closest_peers.size() << " peers to store value. Sending requests..." << std::endl;
        
        MessageWrapper msg;
        auto* store_req = msg.mutable_store_value_req();
        store_req->set_sender_id(routing_table_.get_self_id());
        store_req->set_key(key);
        *store_req->mutable_provider() = provider;

        for (const auto& peer : closest_peers) {
            send(msg, peer.endpoint);
        }
    });
}

void DhtNode::find_value(const std::string& key, std::function<void(const std::vector<PeerInfo>&)> callback) {
    if (storage_.count(key)) {
        boost::asio::post(io_context_, [callback, providers = storage_.at(key)]() {
            callback(providers);
        });
    } else {
        // TODO: Iterative find_value
        boost::asio::post(io_context_, [callback](){ callback({}); });
    }
}

void DhtNode::do_receive() {
    auto sender_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();
    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_), *sender_endpoint,
        [this, sender_endpoint](boost::system::error_code ec, std::size_t length) {
            if (!ec && length > 0) {
                MessageWrapper msg;
                if (msg.ParseFromArray(recv_buffer_.data(), length)) {
                    handle_message(msg, *sender_endpoint);
                }
            }
            do_receive();
        });
}

void DhtNode::send(const MessageWrapper& msg, const boost::asio::ip::udp::endpoint& target) {
    auto serialized_msg = std::make_shared<std::string>();
    msg.SerializeToString(serialized_msg.get());
    std::cout << "[DHT Send] Sending message to " << target << std::endl;
    socket_.async_send_to(boost::asio::buffer(*serialized_msg), target,
        [serialized_msg](boost::system::error_code /*ec*/, std::size_t /*bytes_sent*/) {});
}

void DhtNode::bootstrap(const std::string& host, unsigned short port) {
    boost::asio::ip::udp::resolver resolver(io_context_);
    boost::asio::ip::udp::endpoint bootstrap_endpoint = *resolver.resolve(host, std::to_string(port)).begin();

    MessageWrapper msg;
    auto* find_req = msg.mutable_find_node_req();
    find_req->set_sender_id(routing_table_.get_self_id());
    find_req->set_target_id(routing_table_.get_self_id());

    send(msg, bootstrap_endpoint);
}

void DhtNode::handle_message(const MessageWrapper& msg, const boost::asio::ip::udp::endpoint& sender) {
    std::cout << "[DHT Recv] Received message from " << sender << std::endl;
    std::string sender_id;
    // Сначала извлекаем ID отправителя из любого типа сообщения
    if (msg.has_find_node_req()) sender_id = msg.find_node_req().sender_id();
    else if (msg.has_find_value_req()) sender_id = msg.find_value_req().sender_id();
    else if (msg.has_store_value_req()) sender_id = msg.store_value_req().sender_id();
    
    if (!sender_id.empty()) {
        std::cout << "[DHT] Sender ID is " << dht::to_hex(sender_id) << ". Adding to routing table." << std::endl;
        routing_table_.add_peer({sender_id, sender});
    } else {
        std::cout << "[DHT] Message has no sender_id (it is likely a response)." << std::endl;
    }

    // --- Обработка ОТВЕТОВ на наши запросы ---
    if (msg.has_find_node_res()) {
        std::cout << "[DHT] Handling FindNodeResponse." << std::endl;
        for (const auto& peer_info : msg.find_node_res().neighbors()) {
            if (!peer_info.peer_id().empty()) {
                routing_table_.add_peer({
                    peer_info.peer_id(),
                    boost::asio::ip::udp::endpoint(
                        boost::asio::ip::address::from_string(peer_info.address()),
                        peer_info.port()
                    )
                });
            }
        }
        return; // Ответы не требуют ответа
    }

    // --- Обработка ВХОДЯЩИХ запросов ---
    if (msg.has_find_node_req()) {
        const auto& req = msg.find_node_req();
        std::cout << "[DHT] Handling FindNodeRequest for target " << dht::to_hex(req.target_id()) << std::endl;
        
        MessageWrapper response;
        auto* find_node_res = response.mutable_find_node_res();
        auto closest_peers = routing_table_.find_closest_peers(req.target_id(), 8);
        std::cout << "[DHT] Found " << closest_peers.size() << " closest peers in local table to respond with." << std::endl;
        for (const auto& peer : closest_peers) {
            auto* peer_info = find_node_res->add_neighbors();
            peer_info->set_address(peer.endpoint.address().to_string());
            peer_info->set_port(peer.endpoint.port());
            peer_info->set_peer_id(peer.id);
        }
        send(response, sender);

    } else if (msg.has_find_value_req()) {
        const auto& req = msg.find_value_req();
        std::cout << "[DHT] Handling FindValueRequest for key " << dht::to_hex(req.key()) << std::endl;
        
        MessageWrapper response;
        auto* find_value_res = response.mutable_find_value_res();
        find_value_res->set_key(req.key());

        if (storage_.count(req.key())) {
            auto* peer_list = find_value_res->mutable_providers();
            for (const auto& provider : storage_.at(req.key())) {
                *peer_list->add_peers() = provider;
            }
        } else {
            auto* closer_peers_res = find_value_res->mutable_closer_peers();
            auto closest_peers = routing_table_.find_closest_peers(req.key(), 8);
            for (const auto& peer : closest_peers) {
                auto* peer_info = closer_peers_res->add_neighbors();
                peer_info->set_address(peer.endpoint.address().to_string());
                peer_info->set_port(peer.endpoint.port());
                peer_info->set_peer_id(peer.id);
            }
        }
        send(response, sender);

    } else if (msg.has_store_value_req()) {
        const auto& req = msg.store_value_req();
        std::cout << "[DHT] Handling StoreValueRequest for key " << dht::to_hex(req.key()) << std::endl;
        storage_[req.key()].push_back(req.provider());
    }
}

} // namespace aura
