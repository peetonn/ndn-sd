cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME mfd)
project(${EXECUTABLE_NAME} VERSION 1.0.0)

add_executable(${EXECUTABLE_NAME} main.cpp)

target_link_libraries(${EXECUTABLE_NAME} ndn-sd)