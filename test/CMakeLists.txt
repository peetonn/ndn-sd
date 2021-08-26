include(CheckIncludeFiles)

Include(FetchContent)

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

add_executable(tests ndnsd-test.cpp)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE ndn-sd)

find_package(Bonjour REQUIRED)
target_include_directories(tests PRIVATE ${BONJOUR_INCLUDE_DIR})
target_link_libraries (tests PRIVATE ${BONJOUR_LIBRARY})

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(tests)