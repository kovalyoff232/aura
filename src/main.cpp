#include <iostream>
#include <boost/asio.hpp>
#include "node.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " --listen <port> or --connect <host>:<port>" << std::endl;
        return 1;
    }

    try {
        boost::asio::io_context io_context;
        aura::Node node(io_context);

        if (std::string(argv[1]) == "--listen") {
            short port = (argc > 2) ? std::stoi(argv[2]) : 9090;
            node.listen(port);
            io_context.run();
        } else if (std::string(argv[1]) == "--connect") {
            if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " --connect <host>:<port>" << std::endl;
                return 1;
            }
            std::string host_port = argv[2];
            size_t colon_pos = host_port.find(':');
            if (colon_pos == std::string::npos) {
                std::cerr << "Invalid host:port format" << std::endl;
                return 1;
            }
            std::string host = host_port.substr(0, colon_pos);
            std::string port_str = host_port.substr(colon_pos + 1);
            node.connect(host, port_str);
            io_context.run();
        }

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
