cmake_minimum_required(VERSION 3.25)

project(hse_future LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(hse_future main.cpp)
target_link_libraries(hse_future Threads::Threads)