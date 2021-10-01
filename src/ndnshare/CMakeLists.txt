cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME ndnshare)
project(${EXECUTABLE_NAME} VERSION 0.0.1)

add_executable(${EXECUTABLE_NAME} main.cpp 
              logging.hpp
              key-chain-manager.hpp key-chain-manager.cpp
              mime.hpp mime.cpp
              ndnapp.hpp ndnapp.cpp
              uuid.hpp uuid.cpp)

target_include_directories(${EXECUTABLE_NAME}
                        PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                    )
target_include_directories(${EXECUTABLE_NAME}
                        SYSTEM PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/cli/include>
                    )
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_17)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

# dependencies
# ndn-sd module
target_link_libraries(${EXECUTABLE_NAME} ndn-sd)

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} spdlog::spdlog spdlog::spdlog_header_only)

# docopt
find_package(docopt REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} docopt)
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:docopt>              
               $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

# fmt
find_package(fmt REQUIRED PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fmt ${CMAKE_SOURCE_DIR}/thirdparty/fmt/build)
target_link_libraries(${EXECUTABLE_NAME} fmt::fmt)

# bonjour
#find_package(Bonjour REQUIRED)
#target_include_directories(${EXECUTABLE_NAME} PRIVATE ${BONJOUR_INCLUDE_DIR})
#target_link_libraries (${EXECUTABLE_NAME} ${BONJOUR_LIBRARY})

# ndn-ind
find_package(ndn-ind REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} ndn-ind)
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind>              
               $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

# ndn-ind-tools
find_package(ndn-ind-tools REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} ndn-ind-tools)
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind-tools>              
               $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

# cnl-cpp
find_package(cnl-cpp REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} cnl-cpp)
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:cnl-cpp>              
               $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

# windows-specific
if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME} wsock32 ws2_32)
endif()