# ─────────────────────────────────────────────────────────────────────────────
#  cmake/Catch2Config.cmake
#
#  Exposes catch_discover_tests() from Catch2's bundled Catch.cmake module.
#  Must be included after FetchContent_MakeAvailable(Catch2) has been called
#  in the root CMakeLists.txt, which sets catch2_SOURCE_DIR.
# ─────────────────────────────────────────────────────────────────────────────

if(NOT DEFINED catch2_SOURCE_DIR)
    message(FATAL_ERROR
        "Catch2Config: catch2_SOURCE_DIR is not set. "
        "Call FetchContent_MakeAvailable(Catch2) before including this module.")
endif()

include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
