cmake_minimum_required(VERSION 3.10.0)
project(save_coro VERSION 0.1.0 LANGUAGES C CXX)



# set(CMAKE_CXX_FLAGS -stdlib=libc++)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(save_coro main.cpp)

