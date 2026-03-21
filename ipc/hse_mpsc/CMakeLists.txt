cmake_minimum_required(VERSION 3.25)

project(hse_mpsc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(producer_demo producer_demo.cpp)
add_executable(consumer_demo consumer_demo.cpp)
