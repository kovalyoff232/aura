cmake_minimum_required(VERSION 3.10)

project(Aura CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# --- Protobuf --- 
# Find all .proto files
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

# Generate C++ code from .proto files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# --- Executable ---
add_executable(aura
    src/main.cpp
    src/node.cpp
    src/session.cpp
    src/file_sharer.cpp
    src/dht.cpp
    src/dht_utils.cpp
    ${PROTO_SRCS}
)

# --- Linking libraries ---
target_include_directories(aura PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    include
    include/aura
)

target_link_libraries(aura
    ${PROTOBUF_LIBRARIES}
    ${Boost_LIBRARIES}
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    pthread # May be required for Boost.Asio explicit linking
)