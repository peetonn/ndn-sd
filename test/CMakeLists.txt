include(CheckIncludeFiles)
include(FetchContent)

enable_testing()

set(FETCHCONTENT_QUIET off)
get_filename_component(fc_base "thirdparty"
                       REALPATH BASE_DIR "${CMAKE_PROJECT_DIR}")
set(FETCHCONTENT_BASE_DIR ${fc_base})

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview3
)

FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# ndn-sd unit tests
add_executable(test-ndnsd ndnsd-test.cpp)

target_link_libraries(test-ndnsd PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test-ndnsd PRIVATE ndn-sd)

find_package(Bonjour REQUIRED)
target_include_directories(test-ndnsd PRIVATE ${BONJOUR_INCLUDE_DIR})
target_link_libraries (test-ndnsd PRIVATE ${BONJOUR_LIBRARY})


include(CTest)
include(Catch)
catch_discover_tests(test-ndnsd)

# ndnapp unit tests
add_executable(test-ndnapp key-chain-manager-test.cpp)

target_link_libraries(test-ndnapp PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test-ndnapp PRIVATE ndnapp)

target_include_directories(test-ndnapp
                        PRIVATE
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/ndnapp>
                    )

# copy libraries to bin directory
# ndn-ind
find_package(ndn-ind REQUIRED)
add_custom_command(TARGET test-ndnapp POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind>              
               $<TARGET_FILE_DIR:test-ndnapp>)

# ndn-ind-tools
find_package(ndn-ind-tools REQUIRED)
add_custom_command(TARGET test-ndnapp POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:ndn-ind-tools>              
               $<TARGET_FILE_DIR:test-ndnapp>)

# cnl-cpp
find_package(cnl-cpp REQUIRED)
add_custom_command(TARGET test-ndnapp POST_BUILD 
               COMMAND ${CMAKE_COMMAND} -E copy_if_different
               $<TARGET_FILE:cnl-cpp>              
               $<TARGET_FILE_DIR:test-ndnapp>)

# copy data
add_custom_command(
        TARGET test-ndnapp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/data
                ${CMAKE_CURRENT_BINARY_DIR}/data)


# NOTE: WORKING_DIRECTORY does not work (tested VS2019)
catch_discover_tests(test-ndnapp
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )


