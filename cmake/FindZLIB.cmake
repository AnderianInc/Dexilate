# cmake/FindZLIB.cmake
#
# Project-local override of CMake's built-in FindZLIB.
#
# When zlib was fetched via FetchContent (target: zlibstatic), wire it up
# directly without going through the filesystem search in FindZLIB.cmake.
# That search fails on Windows CI where no system zlib is installed.
#
# Falls back to CMake's built-in FindZLIB for system detection on macOS/Linux.

if(TARGET zlibstatic)
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
    include(FetchContent)
    FetchContent_GetProperties(_dex_zlib)
    set(ZLIB_INCLUDE_DIR  "${_dex_zlib_SOURCE_DIR}")
    set(ZLIB_INCLUDE_DIRS "${_dex_zlib_SOURCE_DIR}" "${_dex_zlib_BINARY_DIR}")
    set(ZLIB_LIBRARIES    ZLIB::ZLIB)
    set(ZLIB_FOUND        TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")
    return()
endif()

include("${CMAKE_ROOT}/Modules/FindZLIB.cmake")
