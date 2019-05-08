cmake_minimum_required(VERSION 3.9.0)
project(lispcpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++17 support has been enabled by default.")

add_executable(lispcpp lispcpp.cpp)

add_executable(test test.cpp)
