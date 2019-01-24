cmake_minimum_required(VERSION 2.8.9)
project(mem_pool)

include_directories(include)

add_subdirectory(src)

add_executable(main main.cpp)

target_link_libraries(main libmempool)