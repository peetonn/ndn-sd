cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME mfd)
project(${EXECUTABLE_NAME} VERSION 1.0.0)

add_executable(${EXECUTABLE_NAME} main.cpp)

target_link_libraries(${EXECUTABLE_NAME} ndn-sd)

find_package(Bonjour REQUIRED)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${BONJOUR_INCLUDE_DIR})
target_link_libraries (${EXECUTABLE_NAME} ${BONJOUR_LIBRARY})

find_package(ndn-ind REQUIRED)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${NDNIND_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${NDNIND_LIBRARIES})

if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME} wsock32 ws2_32)
endif()