#pragma once

#include <boost/asio.hpp>
#include "aura.pb.h"

namespace aura {

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket, uint64_t peer_id);

    void start();

private:
    void do_read();
    void do_write(const MessageWrapper& msg);

    boost::asio::ip::tcp::socket socket_;
    std::vector<uint8_t> read_buffer_;
    uint64_t node_peer_id_;
    bool handshake_done_ = false;
};

}
