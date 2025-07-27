#pragma once

#include <boost/asio.hpp>
#include "aura.pb.h"

namespace aura {

class Node; // Forward declaration

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket, Node& node);

    void start();

    void do_write(const MessageWrapper& msg);

private:
    void do_read();

    boost::asio::ip::tcp::socket socket_;
    std::vector<uint8_t> read_buffer_;
    Node& node_;
    bool handshake_done_ = false;
};

}
