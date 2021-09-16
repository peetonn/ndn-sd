cmake_minimum_required(VERSION 3.10)
project(ndnsd VERSION 1.0.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

include(CheckIncludeFiles)

add_subdirectory(src/ndn-sd)
add_subdirectory(src/ndnshare)
add_subdirectory(test)