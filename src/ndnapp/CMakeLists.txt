cmake_minimum_required(VERSION 3.10)

set(LIBRARY_NAME ndnapp)

set(SOURCES logging.hpp
            identity-manager.hpp identity-manager.cpp
            mime.hpp mime.cpp
            ndnapp.hpp ndnapp.cpp
            uuid.hpp uuid.cpp)

add_library(${LIBRARY_NAME} STATIC ${SOURCES})
target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_17)

# dependencies
# ndn-sd module
target_link_libraries(${LIBRARY_NAME} ndn-sd)

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${LIBRARY_NAME} spdlog::spdlog spdlog::spdlog_header_only)

# fmt
find_package(fmt REQUIRED PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fmt ${CMAKE_SOURCE_DIR}/thirdparty/fmt/build)
target_link_libraries(${LIBRARY_NAME} fmt::fmt)

# ndn-ind
find_package(ndn-ind REQUIRED)
target_link_libraries(${LIBRARY_NAME} ndn-ind)
add_custom_command(TARGET ${LIBRARY_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind>              
               $<TARGET_FILE_DIR:${LIBRARY_NAME}>)

# ndn-ind-tools
find_package(ndn-ind-tools REQUIRED)
target_link_libraries(${LIBRARY_NAME} ndn-ind-tools)
add_custom_command(TARGET ${LIBRARY_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind-tools>              
               $<TARGET_FILE_DIR:${LIBRARY_NAME}>)

# cnl-cpp
find_package(cnl-cpp REQUIRED)
target_link_libraries(${LIBRARY_NAME} cnl-cpp)
add_custom_command(TARGET ${LIBRARY_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:cnl-cpp>              
               $<TARGET_FILE_DIR:${LIBRARY_NAME}>)
