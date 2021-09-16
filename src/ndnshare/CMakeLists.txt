cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME ndnshare)
project(${EXECUTABLE_NAME} VERSION 1.0.0)

add_executable(${EXECUTABLE_NAME} main.cpp)

# add ndn-sd
target_link_libraries(${EXECUTABLE_NAME} ndn-sd)

# add dependencies

# docopt
find_package(docopt REQUIRED)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${DOCOPT_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${DOCOPT_LIBRARIES})

# fmt
find_package(fmt REQUIRED PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fmt ${CMAKE_SOURCE_DIR}/thirdparty/fmt/build)
target_link_libraries(${EXECUTABLE_NAME} fmt::fmt)

# bonjour
#find_package(Bonjour REQUIRED)
#target_include_directories(${EXECUTABLE_NAME} PRIVATE ${BONJOUR_INCLUDE_DIR})
#target_link_libraries (${EXECUTABLE_NAME} ${BONJOUR_LIBRARY})

# ndn-ind
find_package(ndn-ind REQUIRED)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${NDNIND_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${NDNIND_LIBRARIES})

# ndn-ind-tools
find_package(ndn-ind-tools REQUIRED)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${NDNIND_TOOLS_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${NDNIND_TOOLS_LIBRARIES})

# cnl-cpp
find_package(cnl-cpp REQUIRED)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${CNLCPP_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE_NAME} ${CNLCPP_LIBRARIES})

# windows-specific
if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME} wsock32 ws2_32)
endif()