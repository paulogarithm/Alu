cmake_minimum_required(VERSION 3.0)

project(alu)

set(SRCS alu.c)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os")

add_executable(alu ${SRCS})