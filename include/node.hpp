#pragma once

#include "session.hpp"
#include <boost/asio.hpp>
#include <string>
#include <memory>

namespace aura {

class Node {
public:
    Node(boost::asio::io_context& io_context);

    void listen(short port);

    void connect(const std::string& host, const std::string& port);

private:
    void do_accept();

    boost::asio::io_context& io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    uint64_t peer_id_;
};

}
