#include "session.hpp"
#include <iostream>

namespace aura {

Session::Session(boost::asio::ip::tcp::socket socket, uint64_t peer_id)
    : socket_(std::move(socket)), read_buffer_(4096), node_peer_id_(peer_id) {}

void Session::start() {
    std::cout << "Session started with " << socket_.remote_endpoint().address().to_string() << std::endl;
    do_read();
}

void Session::do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                MessageWrapper msg;
                if (msg.ParseFromArray(read_buffer_.data(), length)) {
                    if (msg.has_handshake() && !handshake_done_) {
                        handshake_done_ = true;
                        std::cout << "Received handshake from peer " << msg.handshake().peer_id() << std::endl;

                        // Отвечаем своим рукопожатием
                        aura::MessageWrapper response;
                        auto* handshake = response.mutable_handshake();
                        handshake->set_peer_id(node_peer_id_);
                        handshake->set_version(1);
                        do_write(response);
                    }
                }
                do_read();
            }
        });
}

void Session::do_write(const MessageWrapper& msg) {
    auto self(shared_from_this());
    std::string serialized_msg;
    msg.SerializeToString(&serialized_msg);

    boost::asio::async_write(socket_, boost::asio::buffer(serialized_msg),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << std::endl;
            }
        });
}

}
