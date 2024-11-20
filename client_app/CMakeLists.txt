cmake_minimum_required(VERSION 3.10)

project(WebSocketClientSample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -g)
include_directories(/home/manoj4617/websocketpp)
find_package(Boost REQUIRED system)

add_executable(websocket_client client.cpp)
add_executable(websocket_server server.cpp)

target_link_libraries(websocket_client Boost::system)