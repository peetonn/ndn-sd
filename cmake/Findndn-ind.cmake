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

find_library(NDNIND_IMPLIB_RELEASE
  NAMES ndn-ind
  PATHS
    ${NDNIND_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/VisualStudio/ndn-ind/x64/Release
)
find_library(NDNIND_IMPLIB_DEBUG
  NAMES ndn-ind
  PATHS
    ${NDNIND_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind/VisualStudio/ndn-ind/x64/Debug
)

if (DEFINED NDNIND_IMPLIB_RELEASE)
  set(NDNIND_IMPLIB ${NDNIND_IMPLIB_RELEASE})
  if (NOT NDNIND_IMPLIB_DEBUG)
      set(NDNIND_IMPLIB_DEBUG ${NDNIND_IMPLIB_RELEASE})
  endif()
else()
  if (DEFINED NDNIND_IMPLIB_DEBUG)
    set(NDNIND_IMPLIB ${NDNIND_IMPLIB_DEBUG})
  endif()
endif()

find_file(NDNIND_DLL_RELEASE
          "${CMAKE_SHARED_LIBRARY_PREFIX}ndn-ind${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
          "${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind"
          PATH_SUFFIXES
          VisualStudio/ndn-ind/x64/Release
          NO_DEFAULT_PATH
)

find_file(NDNIND_DLL_DEBUG
          "${CMAKE_SHARED_LIBRARY_PREFIX}ndn-ind${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
          "${CMAKE_SOURCE_DIR}/thirdparty/ndn-ind"
          PATH_SUFFIXES
          VisualStudio/ndn-ind/x64/Debug
          NO_DEFAULT_PATH
)

if (DEFINED NDNIND_DLL_RELEASE)
  set(NDNIND_DLL ${NDNIND_DLL_RELEASE})
    if (NOT NDNIND_DLL_DEBUG)
      set(NDNIND_DLL_DEBUG ${NDNIND_DLL_RELEASE})
    endif()
else()
  if (DEFINED NDNIND_DLL_DEBUG)
    set(NDNIND_DLL ${NDNIND_DLL_DEBUG})
  endif()
endif()

find_package_handle_standard_args(ndn-ind REQUIRED_VARS NDNIND_INCLUDE_DIR NDNIND_IMPLIB NDNIND_DLL)

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

find_package_handle_standard_args(ndn-ind REQUIRED_VARS NDNIND_INCLUDE_DIR NDNIND_LIBRARY)

endif(WIN32)

if( ndn-ind_FOUND )
  set(NDNIND_INCLUDE_DIRS ${NDNIND_INCLUDE_DIR})

  if(WIN32)
    set(NDNIND_LIBRARIES_RELEASE ${NDNIND_IMPLIB_RELEASE})
    set(NDNIND_LIBRARIES_DEBUG ${NDNIND_LIBRARY_DEBUG})
    add_library(ndn-ind SHARED IMPORTED)
    set_target_properties(ndn-ind PROPERTIES
      IMPORTED_LOCATION "${NDNIND_DLL}"
      IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
      IMPORTED_IMPLIB "${NDNIND_IMPLIB}"
      INTERFACE_INCLUDE_DIRECTORIES "${NDNIND_INCLUDE_DIRS}"
    )
    if (DEFINED NDNIND_DLL_DEBUG)
      set_target_properties(ndn-ind PROPERTIES
        IMPORTED_LOCATION_DEBUG "${NDNIND_DLL_DEBUG}"
      )
    endif()
    if (DEFINED NDNIND_IMPLIB_DEBUG)
      set_target_properties(ndn-ind PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${NDNIND_IMPLIB_DEBUG}"
      )
    endif()

  else(WIN32)

    add_library(ndn-ind STATIC IMPORTED)
    set_target_properties(ndn-ind PROPERTIES
      IMPORTED_LOCATION "${NDNIND_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${NDNIND_INCLUDE_DIRS}"
    )

  endif()

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
