#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "aura.pb.h"

namespace aura {

namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class Node; // Forward declaration

class Session : public std::enable_shared_from_this<Session> {
public:
    enum class Type { CLIENT, SERVER };

    // Takes a raw socket and wraps it in an ssl::stream
    Session(tcp::socket socket, Node& node, Type type);
    ~Session();

    void start();
    void stop();

    void do_write(const MessageWrapper& msg);

    // Getter for the socket so Node can use it in async_connect
    ssl::stream<tcp::socket>& get_socket() { return socket_; }

private:
    void do_handshake();
    void do_read();

    ssl::stream<tcp::socket> socket_;
    std::vector<uint8_t> read_buffer_;
    Node& node_;
    Type session_type_;
    bool stopped_ = false;
};

}
