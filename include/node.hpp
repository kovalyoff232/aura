#pragma once

#include "file_sharer.hpp"
#include "session.hpp"
#include <boost/asio.hpp>
#include <string>
#include <memory>

#include <unordered_map>

namespace aura {

class Node {
public:
    Node(boost::asio::io_context& io_context);

    void listen(short port);

    void connect(const std::string& host, const std::string& port);

    void announce_file(const std::string& file_path);

    void send_message(const aura::MessageWrapper& msg);

    uint64_t get_peer_id() const { return peer_id_; }
    FileSharer& get_file_sharer() { return file_sharer_; }

    // Карта <хеш файла, информация о файле>
    std::unordered_map<std::string, FileInfo> available_files_;

    std::shared_ptr<Session> active_session_;

private:
    void do_accept();

    boost::asio::io_context& io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    uint64_t peer_id_;
    FileSharer file_sharer_;
};

}
