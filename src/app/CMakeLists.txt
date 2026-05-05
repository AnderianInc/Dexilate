# ─────────────────────────────────────────────────────────────────────────────
#  src/app/CMakeLists.txt
#  dexilate_app — Application entry point
#
#  Phase 0: GPU-cleared window (Hello Dexilate) using wgpu-native.
#  Links dexilate::platform + wgpu::native only; higher layers added in Phase 1+.
# ─────────────────────────────────────────────────────────────────────────────

if(APPLE)
    add_executable(dexilate_app MACOSX_BUNDLE
        main.cpp
        GpuContext.cpp
    )
else()
    add_executable(dexilate_app
        main.cpp
        GpuContext.cpp
    )
endif()

set_target_properties(dexilate_app PROPERTIES OUTPUT_NAME "dexilate")

target_compile_features(dexilate_app PRIVATE cxx_std_20)

# Local headers (GpuContext.h lives alongside the sources)
target_include_directories(dexilate_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dexilate_app
    PRIVATE
        dexilate::platform
        wgpu::native
)
