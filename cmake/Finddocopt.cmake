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

find_library(DOCOPT_IMPLIB_RELEASE
  NAMES docopt
  PATHS
    ${DOCOPT_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/Release
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/build/Release
)
find_library(DOCOPT_IMPLIB_DEBUG
  NAMES docopt
  PATHS
    ${DOCOPT_BIN}
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/Debug
    ${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp/build/Debug
)
find_file(DOCOPT_DLL_DEBUG
          "${CMAKE_SHARED_LIBRARY_PREFIX}docopt${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
          "${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp"
          PATH_SUFFIXES
          Debug build/Debug
          NO_DEFAULT_PATH
)
find_file(DOCOPT_DLL_RELEASE
          "${CMAKE_SHARED_LIBRARY_PREFIX}docopt${CMAKE_SHARED_LIBRARY_SUFFIX}"
          HINTS
          "${CMAKE_SOURCE_DIR}/thirdparty/docopt.cpp"
          PATH_SUFFIXES
          Release build/Release
          NO_DEFAULT_PATH
)

find_package_handle_standard_args(docopt REQUIRED_VARS DOCOPT_INCLUDE_DIR 
                  DOCOPT_LIBRARY_RELEASE DOCOPT_LIBRARY_DEBUG)

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

find_package_handle_standard_args(docopt REQUIRED_VARS DOCOPT_INCLUDE_DIR DOCOPT_LIBRARY)

endif(WIN32)

if( docopt_FOUND )
  set(DOCOPT_INCLUDE_DIRS ${DOCOPT_INCLUDE_DIR})
  
  if(WIN32)
    set(DOCOPT_LIBRARIES_RELEASE ${DOCOPT_LIBRARY_RELEASE})
    set(DOCOPT_LIBRARIES_DEBUG ${DOCOPT_LIBRARY_DEBUG})
    add_library(docopt SHARED IMPORTED)
    set_target_properties(docopt PROPERTIES
      IMPORTED_LOCATION "${DOCOPT_DLL_RELEASE}"
      IMPORTED_LOCATION_DEBUG "${DOCOPT_DLL_DEBUG}"
      IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
      IMPORTED_IMPLIB "${DOCOPT_LIBRARY_RELEASE}"
      IMPORTED_IMPLIB_DEBUG "${DOCOPT_LIBRARY_DEBUG}"
      INTERFACE_INCLUDE_DIRECTORIES "${DOCOPT_INCLUDE_DIRS}"
    )
  else(WIN32)
    set(DOCOPT_LIBRARIES ${DOCOPT_LIBRARY})
    add_library(docopt SHARED IMPORTED)
    set_target_properties(docopt PROPERTIES
      IMPORTED_LOCATION "${DOCOPT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${DOCOPT_INCLUDE_DIRS}"
    )
  endif()

  set(DOCOPT_FOUND TRUE)

  mark_as_advanced(
    DOCOPT_LIBRARY
    DOCOPT_LIBRARY_RELEASE
    DOCOPT_LIBRARY_DEBUG
    DOCOPT_INCLUDE_DIR
    DOCOPT_DIR
    DOCOPT_BIN
  )
else()
  set( DOCOPT_DIR "" CACHE STRING "An optional hint to a directory for finding includes of `docopt`")
  set( DOCOPT_BIN "" CACHE STRING "An optional hint to a directory for finding binaries of `docopt`")
endif()
