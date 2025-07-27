#include <iostream>
#include <boost/asio.hpp>
#include <cstdio> // For remove()
#include "node.hpp"
#include "file_sharer.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " --listen <port> or --connect <host>:<port>" << std::endl;
        return 1;
    }

    try {
        boost::asio::io_context io_context;
        aura::Node node(io_context);

        if (std::string(argv[1]) == "--listen") {
            aura::Node node(io_context);
            short port = (argc > 2) ? std::stoi(argv[2]) : 9090;
            node.listen(port);

            // "Делимся" тестовым файлом при старте сервера
            std::cout << "[SERVER] Sharing test_file.txt..." << std::endl;
            node.announce_file("/home/kovalyoff/p2p/test_file.txt");

            io_context.run();
        } else if (std::string(argv[1]) == "--share") {
            if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " --share <file_path>" << std::endl;
                return 1;
            }
            aura::FileSharer file_sharer;
            if (file_sharer.share_file(argv[2], node)) {
                std::cout << "Successfully created metadata for " << argv[2] << std::endl;
                // node.announce_file(argv[2]); // Пока не анонсируем
            } else {
                std::cerr << "Failed to create metadata for " << argv[2] << std::endl;
            }
        } else if (std::string(argv[1]) == "--download") {
            if (argc < 4) {
                std::cerr << "Usage: " << argv[0] << " --download <metadata_file> <host>:<port>" << std::endl;
                return 1;
            }

            boost::asio::io_context io_context;
            aura::Node node(io_context);

            // Загружаем метаданные
            aura::FileInfo file_info = node.get_file_sharer().load_metadata(argv[2]);
            if (file_info.file_hash.empty()) {
                std::cerr << "Failed to load metadata from " << argv[2] << std::endl;
                return 1;
            }
            // Устанавливаем путь для сохранения в новый файл
            std::string output_filename = "downloaded_test_file.txt";
            file_info.file_path = output_filename;
            std::cout << "[CLIENT] Will save file to " << output_filename << std::endl;

            // Удаляем старый файл, если он есть, для чистоты эксперимента
            remove(output_filename.c_str());

            // Добавляем файл в список доступных, чтобы сессия могла его найти
            std::string file_hash_str(file_info.file_hash.begin(), file_info.file_hash.end());
            node.available_files_[file_hash_str] = file_info;

            // Подключаемся к пиру
            std::string host_port = argv[3];
            size_t colon_pos = host_port.find(':');
            if (colon_pos == std::string::npos) {
                std::cerr << "Invalid host:port format" << std::endl;
                return 1;
            }
            std::string host = host_port.substr(0, colon_pos);
            std::string port_str = host_port.substr(colon_pos + 1);
            node.connect(host, port_str);

            // Запрашиваем все чанки
            for (uint32_t i = 0; i < file_info.chunk_hashes.size(); ++i) {
                aura::MessageWrapper msg;
                auto* request = msg.mutable_request_chunk();
                request->set_file_hash(file_info.file_hash.data(), file_info.file_hash.size());
                request->set_chunk_index(i);
                node.send_message(msg);
            }

            // Запускаем io_context, чтобы обработать все асинхронные операции
            // и даем ему немного времени на получение всех чанков.
            // FIXME: Это не самый надежный способ. В идеале, нужно отслеживать
            // получение всех чанков и только потом выходить.
            io_context.run_for(std::chrono::seconds(5));

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
