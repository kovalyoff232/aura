#include "node.hpp"
#include "session.hpp"
#include <iostream>
#include <random>

namespace aura {

Node::Node(boost::asio::io_context& io_context)
    : io_context_(io_context) {
    // Генерируем случайный ID для нашего узла
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    peer_id_ = dis(gen);
    std::cout << "Our peer ID: " << peer_id_ << std::endl;
}

void Node::listen(short port) {
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_, 
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
    do_accept();
}

void Node::connect(const std::string& host, const std::string& port) {
    boost::asio::ip::tcp::socket socket(io_context_);
    boost::asio::ip::tcp::resolver resolver(io_context_);
    boost::asio::connect(socket, resolver.resolve(host, port));

    aura::MessageWrapper msg;
    auto* handshake = msg.mutable_handshake();
    handshake->set_peer_id(peer_id_);
    handshake->set_version(1);

    std::string serialized_msg;
    msg.SerializeToString(&serialized_msg);

    boost::asio::write(socket, boost::asio::buffer(serialized_msg));

    std::make_shared<Session>(std::move(socket), peer_id_)->start();
}

void Node::do_accept() {
    acceptor_->async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), peer_id_)->start();
            }
            do_accept();
        });
}

}
