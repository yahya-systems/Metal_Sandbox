cmake_minimum_required(VERSION 3.20)

project(window-manager CXX OBJCXX)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
  message(STATUS "WM : BUILDING WITH APPKIT")
  find_library(APPKIT_LIB AppKit REQUIRED)
  add_library(window-manager STATIC src/mac-os-implementation/window-wrapper.mm)
  target_compile_definitions(window-manager PUBLIC PLATFORM_MACOS)
  target_link_libraries(window-manager PRIVATE ${APPKIT_LIB})

elseif(UNIX AND NOT APPLE)
  message(STATUS "WM : BUILDING WITH XCB")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "linux-implementation/*.cpp")

  add_library(window-manager STATIC ${SOURCES})
  target_compile_definitions(window-manager PUBLIC PLATFORM_LINUX)
  target_link_libraries(window-manager PRIVATE PkgConfig::XCB)

else()
  message(FATAL_ERROR "WM : UNSUPPORTED PLATFORM")

endif()

target_compile_features(window-manager PUBLIC cxx_std_20)
target_include_directories(window-manager PUBLIC include/)
