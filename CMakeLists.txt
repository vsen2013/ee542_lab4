cmake_minimum_required(VERSION 3.14)
project(ee542_lab4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall)

add_executable(client2 client2.cc util.h)
add_executable(server2 server2.cc util.h)

add_executable(tcp_client tcp_client.cc)
add_executable(tcp_server tcp_server.cc)

# add_definitions(-DDEBUG)

target_link_libraries(client pthread)
target_link_libraries(server pthread)