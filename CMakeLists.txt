cmake_minimum_required(VERSION 3.10)
project(optar VERSION 1.0.0)

include (CheckIncludeFiles)

#set(CMAKE_BUILD_TYPE Release)

include_directories(include thirdparty ${PROJECT_BINARY_DIR})

file(GLOB SOURCES "src/*.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

check_include_files (thirdparty/spdlog/spdlog.h HAVE_SPDLOG)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/optar-config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

add_library(optar STATIC ${SOURCES})
