cmake_minimum_required(VERSION 3.20)
project(metal-abstraction CXX OBJCXX)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED) # For CAMetalLayer

find_package(ZLIB REQUIRED)
add_library(tinyexr STATIC tinyExr/tinyexr-implementation.cpp)
target_link_libraries(tinyexr PRIVATE ZLIB::ZLIB)
target_include_directories(tinyexr PUBLIC tinyExr/)

add_subdirectory(Core)
add_subdirectory(png-parser)
add_subdirectory(stb_image)

add_executable(main main.cpp)

find_package(PkgConfig REQUIRED)

add_subdirectory(window-manager)
target_link_libraries(main PRIVATE window-manager)

pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

add_library(gal STATIC src/metalapp.mm)

target_link_libraries(gal PRIVATE tinyexr stb_image)

target_link_libraries(gal PUBLIC 
    ${METAL_FRAMEWORK} 
    ${FOUNDATION_FRAMEWORK} 
    ${QUARTZ_FRAMEWORK} 
    Core
    PkgConfig::GLM
    png-parser
)
target_link_libraries(gal PUBLIC Core )

target_include_directories(gal PUBLIC include/)

target_link_libraries(main PRIVATE gal)

target_compile_features(gal PUBLIC cxx_std_20)
target_compile_features(main PRIVATE cxx_std_20)
