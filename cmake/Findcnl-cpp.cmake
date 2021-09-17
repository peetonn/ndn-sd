# - Try to find cnl-cpp 
#
#  CNLCPP_INCLUDE_DIR - where to find interest.hpp, etc.
#  CNLCPP_LIBRARIES   - List of libraries when using ....
#  CNLCPP_FOUND       - True if cnl-cpp libraries found.

set(CNLCPP_FOUND FALSE)
set(CNLCPP_LIBRARIES)

message(STATUS "Checking whether cnl-cpp is provided...")

include(FindPackageHandleStandardArgs)

# Checks an environment variable; note that the first check
# does not require the usual CMake $-sign.
if( DEFINED ENV{CNLCPP_DIR} )
  set( CNLCPP_DIR "$ENV{CNLCPP_DIR}" )
endif()
if( DEFINED ENV{CNLCPP_BIN} )
  set( CNLCPP_BIN "$ENV{CNLCPP_BIN}" )
endif()

find_path(
  CNLCPP_INCLUDE_DIR
    cnl-cpp/object.hpp
  HINTS
    ${CNLCPP_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/cnl-cpp/include
)

if(WIN32)

find_library(CNLCPP_LIBRARY
  NAMES cnl-cpp
  PATHS
    ${CNLCPP_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/cnl-cpp/VisualStudio/cnl-cpp/x64/Release
)

find_file(CNLCPP_DLL
          "${CMAKE_SHARED_LIBRARY_PREFIX}cnl-cpp${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
          "${CMAKE_SOURCE_DIR}/thirdparty/cnl-cpp"
          PATH_SUFFIXES
          VisualStudio/cnl-cpp/x64/Release
          NO_DEFAULT_PATH
)

else(WIN32)

find_library(CNLCPP_LIBRARY
  NAMES cnl-cpp
  PATHS
    ${CNLCPP_BIN}
    /usr/lib
    /usr/local/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/cnl-cpp/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/cnl-cpp/build/cnl-cpp/lib
)

endif(WIN32)


find_package_handle_standard_args(cnl-cpp REQUIRED_VARS CNLCPP_INCLUDE_DIR CNLCPP_LIBRARY)

if( cnl-cpp_FOUND )
  set(CNLCPP_INCLUDE_DIRS ${CNLCPP_INCLUDE_DIR})

  if(WIN32)
    add_library(cnl-cpp SHARED IMPORTED)
    set_target_properties(cnl-cpp PROPERTIES
      IMPORTED_LOCATION "${CNLCPP_DLL}"
      IMPORTED_CONFIGURATIONS "RELEASE"
      IMPORTED_IMPLIB "${CNLCPP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CNLCPP_INCLUDE_DIRS}"
    )
  else(WIN32)
    add_library(cnl-cpp STATIC IMPORTED)
    set_target_properties(cnl-cpp PROPERTIES
      IMPORTED_LOCATION "${CNLCPP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CNLCPP_INCLUDE_DIRS}"
    )
  endif()

  set(CNLCPP_LIBRARIES ${CNLCPP_LIBRARY})
  set(CNLCPP_FOUND TRUE)

  mark_as_advanced(
    CNLCPP_LIBRARY
    CNLCPP_INCLUDE_DIR
    CNLCPP_DIR
    CNLCPP_BIN
  )
else()
  set( CNLCPP_DIR "" CACHE STRING "An optional hint to a directory for finding includes of `cnl-cpp`")
  set( CNLCPP_BIN "" CACHE STRING "An optional hint to a directory for finding binaries of `cnl-cpp`")
endif()
