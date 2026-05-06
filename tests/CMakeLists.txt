# ─────────────────────────────────────────────────────────────────────────────
#  tests/CMakeLists.txt
#  Dexilate test suite — Catch2 + CTest
#
#  Phase 0: PAL interface tests (window lifecycle, input event dispatch)
#  Phase 1+: tile, PNG codec, brush engine
#  Phase 2+: compositor blend modes, history/undo
#  Phase 3+: path/boolean ops, SVG round-trip
#  Phase 4+: PSD import, color management
# ─────────────────────────────────────────────────────────────────────────────

include(CTest)
include(Catch2Config)

# Helper: add_dexilate_test(name sources...)
# Creates a Catch2 test executable and registers it with CTest.
set(DEXILATE_TEST_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)

function(add_dexilate_test name)
    add_executable(${name} ${DEXILATE_TEST_MAIN} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name}
        PRIVATE
            Catch2::Catch2
            dexilate::platform
    )
    # SKIP_RETURN_CODE 4: Catch2 exits with 4 when all test cases are skipped
    # (SKIP() called before any assertions — e.g. headless CI with no display).
    # CTest treats exit code 4 as "Not Run" rather than "Failed".
    catch_discover_tests(${name} PROPERTIES SKIP_RETURN_CODE 4)
endfunction()

# ── Phase 0 tests ─────────────────────────────────────────────────────────────
add_dexilate_test(test_input_event  platform/test_input_event.cpp)
add_dexilate_test(test_window       platform/test_window.cpp)
