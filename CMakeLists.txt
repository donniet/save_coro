cmake_minimum_required(VERSION 3.10.0)
project(save_coro VERSION 0.1.0 LANGUAGES C CXX)

enable_testing()
include(CTest)

# set(CMAKE_CXX_FLAGS -stdlib=libc++)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS ON)

include_directories(include)

add_subdirectory(test)

add_executable(save_coro main.cpp)

add_executable(test_coro1 test_coro1.cpp)
add_executable(test_interaction test_interaction.cpp)

