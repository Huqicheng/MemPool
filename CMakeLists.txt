cmake_minimum_required(VERSION 2.8.9)
project(mem_pool)

include_directories(include)

SET(libmempool src/MemPool.cpp)
SET(src main.cpp)

ADD_LIBRARY(libmempool ${libSrc})

add_executable(main ${src})

