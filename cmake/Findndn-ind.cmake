# - Try to find ndn-ind 
#
#  NDNIND_INCLUDE_DIR - where to find interest.hpp, etc.
#  NDNIND_LIBRARIES   - List of libraries when using ....
#  NDNIND_FOUND       - True if ndn-ind libraries found.

set(NDNIND_FOUND FALSE)
set(NDNIND_LIBRARIES)

message(STATUS "Checking whether ndn-ind is provided...")

include(FindPackageHandleStandardArgs)

# Checks an environment variable; note that the first check
# does not require the usual CMake $-sign.
if( DEFINED ENV{NDNIND_DIR} )
  set( NDNIND_DIR "$ENV{NDNIND_DIR}" )
endif()
if( DEFINED ENV{NDNIND_BIN} )
  set( NDNIND_BIN "$ENV{NDNIND_BIN}" )
endif()

find_path(
  NDNIND_INCLUDE_DIR
    ndn-ind/interest.hpp
  HINTS
    ${NDNIND_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/include
)

if(WIN32)

find_library(NDNIND_LIBRARY
  NAMES ndn-ind
  PATHS
    ${NDNIND_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/VisualStudio/ndn-ind/x64/Release
)

else(WIN32)

find_library(NDNIND_LIBRARY
  NAMES ndn-ind
  PATHS
    ${NDNIND_BIN}
    /usr/lib
    /usr/local/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/build/ndn-ind/lib
)

endif(WIN32)


find_package_handle_standard_args(ndn-ind REQUIRED_VARS NDNIND_INCLUDE_DIR NDNIND_LIBRARY)

if( ndn-ind_FOUND )
  set(NDNIND_INCLUDE_DIRS ${NDNIND_INCLUDE_DIR})
  set(NDNIND_LIBRARIES ${NDNIND_LIBRARY})
  set(NDNIND_FOUND TRUE)

  mark_as_advanced(
    NDNIND_LIBRARY
    NDNIND_INCLUDE_DIR
    NDNIND_DIR
    NDNIND_BIN
  )
else()
  set( NDNIND_DIR "" CACHE STRING "An optional hint to a directory for finding includes of `ndn-ind`")
  set( NDNIND_BIN "" CACHE STRING "An optional hint to a directory for finding binaries of `ndn-ind`")
endif()
