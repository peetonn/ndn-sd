# - Try to find ndn-ind-tools 
#
#  NDNIND_TOOLS_INCLUDE_DIR - where to find interest.hpp, etc.
#  NDNIND_TOOLS_LIBRARIES   - List of libraries when using ....
#  NDNIND_TOOLS_FOUND       - True if ndn-ind-tools libraries found.

set(NDNIND_TOOLS_FOUND FALSE)
set(NDNIND_TOOLS_LIBRARIES)

message(STATUS "Checking whether ndn-ind-tools is provided...")

include(FindPackageHandleStandardArgs)

# Checks an environment variable; note that the first check
# does not require the usual CMake $-sign.
if( DEFINED ENV{NDNIND_TOOLS_DIR} )
  set( NDNIND_TOOLS_DIR "$ENV{NDNIND_TOOLS_DIR}" )
endif()
if( DEFINED ENV{NDNIND_TOOLS_BIN} )
  set( NDNIND_TOOLS_BIN "$ENV{NDNIND_TOOLS_BIN}" )
endif()

find_path(
  NDNIND_TOOLS_INCLUDE_DIR
    ndn-ind-tools/micro-forwarder/micro-forwarder.hpp
  HINTS
    ${NDNIND_TOOLS_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/include
)

if(WIN32)

find_library(NDNIND_TOOLS_LIBRARY
  NAMES ndn-ind-tools
  PATHS
    ${NDNIND_TOOLS_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/VisualStudio/ndn-ind/x64/Release
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/VisualStudio/ndn-ind/ndn-ind-tools/x64/Release
)

else(WIN32)

find_library(NDNIND_TOOLS_LIBRARY
  NAMES ndn-ind-tools
  PATHS
    ${NDNIND_TOOLS_BIN}
    /usr/lib
    /usr/local/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/build/ndn-ind/lib
)

endif(WIN32)


find_package_handle_standard_args(ndn-ind-tools REQUIRED_VARS NDNIND_TOOLS_INCLUDE_DIR NDNIND_TOOLS_LIBRARY)

if( ndn-ind-tools_FOUND )
  set(NDNIND_TOOLS_INCLUDE_DIRS ${NDNIND_TOOLS_INCLUDE_DIR})
  set(NDNIND_TOOLS_LIBRARIES ${NDNIND_TOOLS_LIBRARY})
  set(NDNIND_TOOLS_FOUND TRUE)

  mark_as_advanced(
    NDNIND_TOOLS_LIBRARY
    NDNIND_TOOLS_INCLUDE_DIR
    NDNIND_TOOLS_DIR
    NDNIND_TOOLS_BIN
  )
else()
  set( NDNIND_TOOLS_DIR "" CACHE STRING "An optional hint to a directory for finding includes of `ndn-ind-tools`")
  set( NDNIND_TOOLS_BIN "" CACHE STRING "An optional hint to a directory for finding binaries of `ndn-ind-tools`")
endif()
