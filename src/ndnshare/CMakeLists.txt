cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME ndnshare)
project(${EXECUTABLE_NAME} VERSION 0.0.1)

add_executable(${EXECUTABLE_NAME} 
              main.cpp
              fileshare.hpp fileshare.cpp)

target_include_directories(${EXECUTABLE_NAME}
                        PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/ndnapp>
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

# ndnapp module
target_link_libraries(${EXECUTABLE_NAME} ndnapp)

# docopt
find_package(docopt REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} docopt)
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:docopt>              
               $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

