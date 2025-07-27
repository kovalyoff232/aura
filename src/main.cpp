#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <string>
#include <vector>

#include "node.hpp"

// Prototype for the function to connect to a bootstrap node
void bootstrap_node(aura::Node& node, const std::string& host_port_str);

// The main application logic will now be here
int main(int argc, char* argv[]) {
    try {
        // --- Command line argument parsing ---
        short port = 9090;
        std::string bootstrap_peer;
        std::string connect_peer; // New argument for direct TCP connection
        std::string file_to_share;
        std::string hash_to_download;

        std::vector<std::string> args(argv + 1, argv + argc);
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--port" && i + 1 < args.size()) {
                port = std::stoi(args[++i]);
            } else if (args[i] == "--bootstrap" && i + 1 < args.size()) {
                bootstrap_peer = args[++i];
            } else if (args[i] == "--connect" && i + 1 < args.size()) {
                connect_peer = args[++i];
            } else if (args[i] == "--share" && i + 1 < args.size()) {
                file_to_share = args[++i];
            } else if (args[i] == "--download" && i + 1 < args.size()) {
                hash_to_download = args[++i];
            } else if (args[i] == "--help") {
                std::cout << "Usage: " << argv[0] << " [--port <port>] [--bootstrap <host:port>] [--connect <host:port>] [--share <file>] [--download <hash>]" << std::endl;
                return 0;
            }
        }

        // --- Node Initialization ---
        boost::asio::io_context io_context;
        aura::Node node(io_context, port, port);
        node.listen(port);

        std::cout << "Aura node started." << std::endl;
        std::cout << "Listening on TCP/UDP port " << port << std::endl;

        // If --connect is specified, establish a direct TCP connection
        if (!connect_peer.empty()) {
            size_t colon_pos = connect_peer.find(':');
            if (colon_pos != std::string::npos) {
                std::string host = connect_peer.substr(0, colon_pos);
                std::string port_str = connect_peer.substr(colon_pos + 1);
                std::cout << "Connecting to " << host << ":" << port_str << "..." << std::endl;
                node.connect(host, port_str);
            } else {
                std::cerr << "Invalid host:port format for connect peer: " << connect_peer << std::endl;
            }
        }

        // If a bootstrap node is specified, connect to the DHT network through it
        if (!bootstrap_peer.empty()) {
            std::cout << "Bootstrapping with " << bootstrap_peer << "..." << std::endl;
            bootstrap_node(node, bootstrap_peer);

            // If a file needs to be shared, do it 5 seconds after bootstrap
            if (!file_to_share.empty()) {
                auto timer = std::make_shared<boost::asio::steady_timer>(io_context, std::chrono::seconds(5));
                timer->async_wait([&node, file_to_share, timer](const boost::system::error_code& ec) {
                    if (!ec) {
                        std::cout << "Announcing file " << file_to_share << "..." << std::endl;
                        node.announce_file(file_to_share);
                    }
                });
            }
        }

        // --- Execute startup commands ---
        // These commands are executed asynchronously. We just "post" them to the io_context
        // so they run after the node is fully ready.
        if (!file_to_share.empty() && bootstrap_peer.empty()) {
            boost::asio::post(io_context, [&node, file_to_share]() {
                node.announce_file(file_to_share);
            });
        }

        if (!hash_to_download.empty()) {
            // A small delay before searching to allow time to get a response from the bootstrap node
            auto timer = std::make_shared<boost::asio::steady_timer>(io_context, std::chrono::seconds(3));
            timer->async_wait([&node, hash_to_download, timer](const boost::system::error_code& ec) {
                if (!ec) {
                    node.download_file(hash_to_download);
                }
            });
        }

        // --- Start the main event processing loop ---
        io_context.run();

    } catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void bootstrap_node(aura::Node& node, const std::string& host_port_str) {
    size_t colon_pos = host_port_str.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid host:port format for bootstrap node: " << host_port_str << std::endl;
        return;
    }
    std::string host = host_port_str.substr(0, colon_pos);
    short bootstrap_port = std::stoi(host_port_str.substr(colon_pos + 1));
    
    if (node.get_dht_node()) {
        node.get_dht_node()->bootstrap(host, bootstrap_port);
    } else {
        std::cerr << "DHT node is not initialized, cannot bootstrap." << std::endl;
    }
}
