cmake_minimum_required(VERSION 3.20)
project(core-lib CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(Core STATIC src/log.cpp)

target_include_directories(Core PUBLIC include/)

