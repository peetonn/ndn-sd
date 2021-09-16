# - Try to find docopt
#
#  DOCOPT_INCLUDE_DIR - where to find docopt.h
#  DOCOPT_LIBRARIES   - List of libraries when using ....
#  DOCOPT_FOUND       - True if docopt library found.

set(DOCOPT_FOUND FALSE)
set(DOCOPT_LIBRARIES)

message(STATUS "Checking whether docopt is provided...")

include(FindPackageHandleStandardArgs)

# Checks an environment variable; note that the first check
# does not require the usual CMake $-sign.
if( DEFINED ENV{DOCOPT_DIR} )
  set( DOCOPT_DIR "$ENV{DOCOPT_DIR}" )
endif()
if( DEFINED ENV{DOCOPT_BIN} )
  set( DOCOPT_BIN "$ENV{DOCOPT_BIN}" )
endif()

find_path(
  DOCOPT_INCLUDE_DIR
    docopt.h
  HINTS
    ${DOCOPT_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp
)

if(WIN32)

find_library(DOCOPT_LIBRARY
  NAMES docopt
  PATHS
    ${DOCOPT_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/Release
)

else(WIN32)

find_library(DOCOPT_LIBRARY
  NAMES docopt
  PATHS
    ${DOCOPT_BIN}
    /usr/lib
    /usr/local/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/build/lib
)

endif(WIN32)


find_package_handle_standard_args(docopt REQUIRED_VARS DOCOPT_INCLUDE_DIR DOCOPT_LIBRARY)

if( docopt_FOUND )
  set(DOCOPT_INCLUDE_DIRS ${DOCOPT_INCLUDE_DIR})
  set(DOCOPT_LIBRARIES ${DOCOPT_LIBRARY})
  set(DOCOPT_FOUND TRUE)

  mark_as_advanced(
    DOCOPT_LIBRARY
    DOCOPT_INCLUDE_DIR
    DOCOPT_DIR
    DOCOPT_BIN
  )
else()
  set( DOCOPT_DIR "" CACHE STRING "An optional hint to a directory for finding includes of `docopt`")
  set( DOCOPT_BIN "" CACHE STRING "An optional hint to a directory for finding binaries of `docopt`")
endif()
