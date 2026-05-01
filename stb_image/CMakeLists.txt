cmake_minimum_required(VERSION 3.20)
project(stb_image CXX)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(stb_image STATIC stb_image_implementation.cpp)

target_include_directories(stb_image PUBLIC include/)

target_compile_features(stb_image PUBLIC cxx_std_20)
