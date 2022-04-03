cmake_minimum_required(VERSION 3.21)
project(MemoryTools)

set(CMAKE_CXX_STANDARD 14)


add_executable(SocketDemo main.cpp)
add_executable(BIO BIO.cpp)
add_executable(NIO NIO.cpp)
add_executable(SELECT select.cpp)
add_executable(EPOLL epoll.cpp)

