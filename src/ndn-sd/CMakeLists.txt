cmake_minimum_required(VERSION 3.10)

set(LIBRARY_NAME ndn-sd)
project(${LIBRARY_NAME} VERSION 1.0.0)


set(SOURCES ndn-sd.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include/${LIBRARY_NAME}/ndn-sd.hpp)
add_library(${LIBRARY_NAME} STATIC ${SOURCES})

target_include_directories(${LIBRARY_NAME}
                        PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/${LIBRARY_NAME}>
                        $<INSTALL_INTERFACE:include/${LIBRARY_NAME}>
                        PUBLIC
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
                        $<INSTALL_INTERFACE:include>
                    )

target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_17)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

find_package(Bonjour REQUIRED)
target_include_directories(${LIBRARY_NAME} PRIVATE ${BONJOUR_INCLUDE_DIR})
target_link_libraries (${LIBRARY_NAME} ${BONJOUR_LIBRARY})

find_package(ndn-ind REQUIRED)
target_include_directories(${LIBRARY_NAME} SYSTEM PUBLIC ${NDNIND_INCLUDE_DIRS})
target_link_libraries(${LIBRARY_NAME} ${NDNIND_LIBRARIES})

if(WIN32)
  target_link_libraries(${LIBRARY_NAME} wsock32 ws2_32)
endif()