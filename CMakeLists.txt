cmake_minimum_required(VERSION 3.10)

project(Aura CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Зависимости ---
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# --- Protobuf --- 
# Находим все .proto файлы
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

# Генерируем C++ код из .proto файлов
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# --- Исполняемый файл ---
add_executable(aura
    src/main.cpp
    src/node.cpp
    src/session.cpp
    src/file_sharer.cpp
    ${PROTO_SRCS}
)

# --- Подключение библиотек ---
target_include_directories(aura PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    include
)

target_link_libraries(aura
    ${PROTOBUF_LIBRARIES}
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    pthread # Для Boost.Asio может потребоваться явное указание
)
