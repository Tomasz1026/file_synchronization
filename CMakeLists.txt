cmake_minimum_required(VERSION 3.10)
project(file_synchronization)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(server server/server.cpp)
add_executable(client client/client.cpp)

configure_file(server/s_config.txt ${CMAKE_CURRENT_BINARY_DIR}/server_config.txt COPYONLY)
configure_file(client/c_config.txt ${CMAKE_CURRENT_BINARY_DIR}/client_config.txt COPYONLY)
