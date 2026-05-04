# ─────────────────────────────────────────────────────────────────────────────
#  cmake/Packaging.cmake
#  CPack configuration for Canvas release packaging.
#
#  Generates:
#    macOS   → Canvas-<ver>-macOS.dmg         (notarized, universal binary)
#    Windows → Canvas-<ver>-Windows-x64.exe   (NSIS installer, code-signed)
#    Linux   → Canvas-<ver>-Linux-x64.AppImage
#              Canvas-<ver>-Linux-amd64.deb
# ─────────────────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ── Install rules ─────────────────────────────────────────────────────────────
install(TARGETS canvas_app
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE   DESTINATION .                       # macOS .app bundle
)

# Qt6 runtime deployment is handled per-platform below.

# ─────────────────────────────────────────────────────────────────────────────
#  CPack global settings
# ─────────────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "Canvas")
set(CPACK_PACKAGE_VENDOR            "Canvas Software")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Professional creative suite — raster + vector")
set(CPACK_PACKAGE_DESCRIPTION_FILE  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR     ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR     ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH     ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://your-canvas-site.com")

# ─────────────────────────────────────────────────────────────────────────────
#  macOS — DragNDrop .dmg
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    # macOS app bundle properties (set on the target in src/app/CMakeLists.txt)
    set(MACOSX_BUNDLE_GUI_IDENTIFIER     "com.canvas.app")
    set(MACOSX_BUNDLE_BUNDLE_VERSION     "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT          "Copyright © 2026 Canvas Software")

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME   "Canvas ${PROJECT_VERSION}")
    set(CPACK_DMG_BACKGROUND_IMAGE
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/packaging/macos_dmg_bg.png")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_dmg_setup.scpt")
    set(CPACK_PACKAGE_FILE_NAME "Canvas-${PROJECT_VERSION}-macOS")

    # Deploy Qt frameworks into the bundle before packaging.
    # Requires macdeployqt from the Qt installation.
    find_program(MACDEPLOYQT_EXEC macdeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        REQUIRED
    )
    install(CODE "
        message(STATUS \"Running macdeployqt...\")
        execute_process(
            COMMAND \"${MACDEPLOYQT_EXEC}\"
                    \"\${CMAKE_INSTALL_PREFIX}/Canvas.app\"
                    -always-overwrite
            RESULT_VARIABLE _exit
        )
        if(NOT _exit EQUAL 0)
            message(FATAL_ERROR \"macdeployqt failed (exit code \${_exit})\")
        endif()
    " COMPONENT Runtime)

    # Notarization (run manually post-package or via CI):
    # xcrun notarytool submit Canvas-x.y.z-macOS.dmg \
    #     --apple-id $APPLE_ID --team-id $TEAM_ID --password $APP_PASSWORD --wait
    # xcrun stapler staple Canvas-x.y.z-macOS.dmg

# ─────────────────────────────────────────────────────────────────────────────
#  Windows — NSIS installer
# ─────────────────────────────────────────────────────────────────────────────
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME      "Canvas ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME      "Canvas")
    set(CPACK_NSIS_INSTALL_ROOT      "$PROGRAMFILES64")
    set(CPACK_NSIS_URL_INFO_ABOUT    "https://your-canvas-site.com")
    set(CPACK_NSIS_HELP_LINK         "https://your-canvas-site.com/support")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH       ON)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Canvas")
    set(CPACK_PACKAGE_FILE_NAME      "Canvas-${PROJECT_VERSION}-Windows-x64")

    # Desktop shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\Canvas.lnk' '$INSTDIR\\\\bin\\\\canvas.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\Canvas.lnk'")

    # File associations (.canvas files)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.canvas' '' 'CanvasDocument'
        WriteRegStr HKCR 'CanvasDocument' '' 'Canvas Document'
        WriteRegStr HKCR 'CanvasDocument\\\\DefaultIcon' '' '$INSTDIR\\\\bin\\\\canvas.exe,0'
        WriteRegStr HKCR 'CanvasDocument\\\\shell\\\\open\\\\command' ''
                   '\"$INSTDIR\\\\bin\\\\canvas.exe\" \"%1\"'
    ")

    # Deploy Qt DLLs alongside the exe.
    find_program(WINDEPLOYQT_EXEC windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        REQUIRED
    )
    install(CODE "
        message(STATUS \"Running windeployqt...\")
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXEC}\"
                    --release
                    \"\${CMAKE_INSTALL_PREFIX}/bin/canvas.exe\"
            RESULT_VARIABLE _exit
        )
        if(NOT _exit EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed (exit code \${_exit})\")
        endif()
    " COMPONENT Runtime)

# ─────────────────────────────────────────────────────────────────────────────
#  Linux — DEB + AppImage
# ─────────────────────────────────────────────────────────────────────────────
elseif(UNIX AND NOT APPLE)
    # DEB package
    set(CPACK_GENERATOR           "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "canvas")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Canvas Software <hello@your-canvas-site.com>")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Canvas — professional creative suite (raster + vector image editor)")
    set(CPACK_DEBIAN_PACKAGE_SECTION    "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libgl1, libglib2.0-0, libfontconfig1, libfreetype6, libx11-6")
    set(CPACK_PACKAGE_FILE_NAME "Canvas-${PROJECT_VERSION}-Linux-amd64")

    # AppImage is built separately by CI using linuxdeployqt/appimagetool.
    # See .github/workflows/release.yml for the AppImage build step.
    # The install tree produced by 'cmake --install' is the AppImage input.
endif()

# ─────────────────────────────────────────────────────────────────────────────
include(CPack)
