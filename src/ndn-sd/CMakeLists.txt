cmake_minimum_required(VERSION 3.10)

set(LIBRARY_NAME ndn-sd)
project(${LIBRARY_NAME} VERSION 1.0.0)

file(GLOB SOURCES "*.cpp")
add_library(${LIBRARY_NAME} STATIC ${SOURCES})

target_include_directories(${LIBRARY_NAME}
                        PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/${LIBRARY_NAME}>
                        $<INSTALL_INTERFACE:include/${LIBRARY_NAME}>
                        PUBLIC
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
                        $<INSTALL_INTERFACE:include>
                    )
target_include_directories(${LIBRARY_NAME} PRIVATE SYSTEM
                        ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ndn-ind/include
                    )
target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_17)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)
