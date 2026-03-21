cmake_minimum_required(VERSION 3.25)

project(hse_mutex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(hse_mutex hse_mutex.cpp)
target_link_libraries(hse_mutex PRIVATE Threads::Threads)
target_compile_options(hse_mutex PRIVATE -fsanitize=thread)
target_link_options(hse_mutex PRIVATE -fsanitize=thread)