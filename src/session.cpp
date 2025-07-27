#include "session.hpp"
#include "node.hpp"
#include <iostream>

namespace aura {

Session::Session(boost::asio::ip::tcp::socket socket, Node& node)
    : socket_(std::move(socket)), read_buffer_(4096), node_(node) {}

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
                        handshake->set_peer_id(node_.get_peer_id());
                        handshake->set_version(1);
                        do_write(response);
                    } else if (msg.has_announce()) {
                        const auto& announce = msg.announce();
                        std::cout << "Received announce for file " << announce.file_hash() << std::endl;
                        // TODO: Сохранить информацию о файле
                    } else if (msg.has_request_chunk()) {
                        const auto& request = msg.request_chunk();
                        std::string file_hash_str(request.file_hash().begin(), request.file_hash().end());
                        std::cout << "[SERVER] Received request for chunk " << request.chunk_index() 
                                  << " of file " << request.file_hash() << std::endl;

                        if (node_.available_files_.count(file_hash_str)) {
                            std::cout << "[SERVER] File found. Preparing chunk." << std::endl;
                            const auto& file_info = node_.available_files_.at(file_hash_str);
                            std::vector<uint8_t> chunk_data = node_.get_file_sharer().get_chunk(file_info, request.chunk_index());

                            aura::MessageWrapper response;
                            auto* chunk_msg = response.mutable_send_chunk();
                            chunk_msg->set_file_hash(request.file_hash());
                            chunk_msg->set_chunk_index(request.chunk_index());
                            chunk_msg->set_data(chunk_data.data(), chunk_data.size());
                            
                            std::cout << "[SERVER] Sending chunk " << request.chunk_index() << " with size " << chunk_data.size() << " bytes." << std::endl;
                            do_write(response);
                        } else {
                            std::cerr << "[SERVER] File hash not found!" << std::endl;
                        }
                    } else if (msg.has_send_chunk()) {
                        const auto& chunk = msg.send_chunk();
                        std::string file_hash_str(chunk.file_hash().begin(), chunk.file_hash().end());
                        std::cout << "[CLIENT] Received chunk " << chunk.chunk_index() << " of file " << chunk.file_hash() << std::endl;

                        if (node_.available_files_.count(file_hash_str)) {
                            const auto& file_info = node_.available_files_.at(file_hash_str);
                            std::vector<uint8_t> data(chunk.data().begin(), chunk.data().end());
                            std::cout << "[CLIENT] Saving chunk " << chunk.chunk_index() << " with size " << data.size() << " bytes." << std::endl;
                            node_.get_file_sharer().save_chunk(file_info, chunk.chunk_index(), data);
                        } else {
                            std::cerr << "[CLIENT] Received chunk for an unknown file hash!" << std::endl;
                        }
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
