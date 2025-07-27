#include "session.hpp"
#include "node.hpp"
#include <iostream>

namespace aura {

// The constructor now takes the session type (client or server)
Session::Session(tcp::socket socket, Node& node, Type type)
    : socket_(std::move(socket), node.get_ssl_context()),
      read_buffer_(4096), // 4KB read buffer
      node_(node),
      session_type_(type) {}

Session::~Session() {
    // For debugging
    // std::cout << "Session destroyed." << std::endl;
}

void Session::start() {
    // Start the SSL handshake
    do_handshake();
}

void Session::stop() {
    if (stopped_) {
        return;
    }
    stopped_ = true;

    // Gracefully shut down the SSL connection
    if (socket_.lowest_layer().is_open()) {
        boost::system::error_code ec;
        socket_.async_shutdown([self = shared_from_this()](const boost::system::error_code& ec) {
            // After shutdown, we can safely close the socket
            if (self->socket_.lowest_layer().is_open()) {
                self->socket_.lowest_layer().close();
            }
        });
    }
    
    // Remove ourselves from the active session list in Node.
    node_.remove_session(shared_from_this());
}

void Session::do_handshake() {
    auto self(shared_from_this());
    // Use the correct handshake type depending on the session type
    auto handshake_type = (session_type_ == Type::CLIENT) ? 
                          ssl::stream_base::client : 
                          ssl::stream_base::server;

    socket_.async_handshake(handshake_type,
        [this, self](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "SSL Handshake successful." << std::endl;
                
                // The client should send its Handshake first
                if (session_type_ == Type::CLIENT) {
                    std::cout << "Acting as CLIENT: sending initial handshake." << std::endl;
                    aura::MessageWrapper msg;
                    auto* handshake = msg.mutable_handshake();
                    handshake->set_peer_id(node_.get_peer_id());
                    handshake->set_version(1);
                    do_write(msg);
                } else {
                    std::cout << "Acting as SERVER: waiting for handshake." << std::endl;
                }

                // Start reading data
                do_read();
            } else {
                std::cerr << "SSL Handshake failed: " << ec.message() << std::endl;
                stop();
            }
        });
}

void Session::do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                MessageWrapper msg;
                if (msg.ParseFromArray(read_buffer_.data(), length)) {
                    if (msg.has_handshake()) {
                        std::cout << "Received encrypted handshake from a peer." << std::endl;

                        // If we are the server, respond to the handshake
                        if (session_type_ == Type::SERVER) {
                            std::cout << "Acting as SERVER: responding to handshake." << std::endl;
                            aura::MessageWrapper response;
                            auto* handshake = response.mutable_handshake();
                            handshake->set_peer_id(node_.get_peer_id());
                            handshake->set_version(1);
                            do_write(response);
                        }
                    } 
                    // ... (rest of the logic for handling announce, request_chunk, etc.)
                } else {
                    std::cerr << "Failed to parse message." << std::endl;
                }
                do_read(); // Continue reading
            } else {
                if (ec != boost::asio::error::eof) {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                }
                stop();
            }
        });
}

void Session::do_write(const MessageWrapper& msg) {
    auto self(shared_from_this());
    // Use a shared_ptr for the buffer so it lives until the write is complete
    auto serialized_msg = std::make_shared<std::string>();
    msg.SerializeToString(serialized_msg.get());

    boost::asio::async_write(socket_, boost::asio::buffer(*serialized_msg),
        [this, self, serialized_msg](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << std::endl;
                stop(); // Stop the session on a write error
            }
        });
}

}
