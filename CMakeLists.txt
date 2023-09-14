cmake_minimum_required(VERSION 3.14)
project(ee542_lab4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall)

add_executable(client client.cc util.h)
add_executable(server server.cc util.h)

target_link_libraries(client crypto pthread)