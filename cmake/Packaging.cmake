# ─────────────────────────────────────────────────────────────────────────────
#  cmake/Packaging.cmake
#  CPack configuration for Dexilate release packaging.
#
#  Generates:
#    macOS   → Dexilate-<ver>-macOS.dmg         (notarized, universal binary)
#    Windows → Dexilate-<ver>-Windows-x64.exe   (NSIS installer, code-signed)
#    Linux   → Dexilate-<ver>-Linux-x64.AppImage
#              Dexilate-<ver>-Linux-amd64.deb
#
#  Configurable CMake cache variables (override in CI or cmake command line):
#    DEXILATE_HOMEPAGE_URL    — project website
#    DEXILATE_SUPPORT_EMAIL   — maintainer contact for .deb packages
# ─────────────────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ── Configurable URLs/contacts ────────────────────────────────────────────────
set(DEXILATE_HOMEPAGE_URL  "https://github.com/your-org/dexilate"
    CACHE STRING "Dexilate project homepage URL")
set(DEXILATE_SUPPORT_EMAIL "hello@dexilate-app.example"
    CACHE STRING "Maintainer email for Linux .deb packages")

# ── Install rules ─────────────────────────────────────────────────────────────
install(TARGETS dexilate_app
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE   DESTINATION .                       # macOS .app bundle
)

# Qt6 runtime deployment is handled per-platform below.

# ─────────────────────────────────────────────────────────────────────────────
#  CPack global settings
# ─────────────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "Dexilate")
set(CPACK_PACKAGE_VENDOR            "Dexilate Software")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Professional creative suite — raster + vector")
set(CPACK_PACKAGE_DESCRIPTION_FILE  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR     ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR     ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH     ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL      "${DEXILATE_HOMEPAGE_URL}")

# ─────────────────────────────────────────────────────────────────────────────
#  macOS — DragNDrop .dmg
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    # macOS app bundle properties (set on the target in src/app/CMakeLists.txt)
    set(MACOSX_BUNDLE_GUI_IDENTIFIER     "com.dexilate.app")
    set(MACOSX_BUNDLE_BUNDLE_VERSION     "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT          "Copyright © 2026 Dexilate Software")

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME   "Dexilate ${PROJECT_VERSION}")
    set(CPACK_DMG_BACKGROUND_IMAGE
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/packaging/macos_dmg_bg.png")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_dmg_setup.scpt")
    set(CPACK_PACKAGE_FILE_NAME "Dexilate-${PROJECT_VERSION}-macOS")

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
                    \"\${CMAKE_INSTALL_PREFIX}/Dexilate.app\"
                    -always-overwrite
            RESULT_VARIABLE _exit
        )
        if(NOT _exit EQUAL 0)
            message(FATAL_ERROR \"macdeployqt failed (exit code \${_exit})\")
        endif()
    " COMPONENT Runtime)

    # Notarization (run via CI using GitHub Actions secrets):
    #   xcrun notarytool submit Dexilate-x.y.z-macOS.dmg \
    #       --apple-id $APPLE_ID --team-id $TEAM_ID --password $APP_PASSWORD --wait
    #   xcrun stapler staple Dexilate-x.y.z-macOS.dmg
    # See .github/workflows/release.yml for the automated notarization step.

# ─────────────────────────────────────────────────────────────────────────────
#  Windows — NSIS installer
# ─────────────────────────────────────────────────────────────────────────────
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME      "Dexilate ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME      "Dexilate")
    set(CPACK_NSIS_INSTALL_ROOT      "$PROGRAMFILES64")
    set(CPACK_NSIS_URL_INFO_ABOUT    "${DEXILATE_HOMEPAGE_URL}")
    set(CPACK_NSIS_HELP_LINK         "${DEXILATE_HOMEPAGE_URL}/support")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH       ON)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Dexilate")
    set(CPACK_PACKAGE_FILE_NAME      "Dexilate-${PROJECT_VERSION}-Windows-x64")

    # Desktop shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\Dexilate.lnk' '$INSTDIR\\\\bin\\\\dexilate.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\Dexilate.lnk'")

    # File associations (.canvas files)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.canvas' '' 'DexilateDocument'
        WriteRegStr HKCR 'DexilateDocument' '' 'Dexilate Document'
        WriteRegStr HKCR 'DexilateDocument\\\\DefaultIcon' '' '$INSTDIR\\\\bin\\\\dexilate.exe,0'
        WriteRegStr HKCR 'DexilateDocument\\\\shell\\\\open\\\\command' ''
                   '\"$INSTDIR\\\\bin\\\\dexilate.exe\" \"%1\"'
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
                    \"\${CMAKE_INSTALL_PREFIX}/bin/dexilate.exe\"
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
    set(CPACK_DEBIAN_PACKAGE_NAME "dexilate")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dexilate Software <${DEXILATE_SUPPORT_EMAIL}>")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Dexilate — professional creative suite (raster + vector image editor)")
    set(CPACK_DEBIAN_PACKAGE_SECTION    "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libgl1, libglib2.0-0, libfontconfig1, libfreetype6, libx11-6")
    set(CPACK_PACKAGE_FILE_NAME "Dexilate-${PROJECT_VERSION}-Linux-amd64")

    # AppImage is built separately by CI using linuxdeployqt/appimagetool.
    # See .github/workflows/release.yml for the AppImage build step.
    # The install tree produced by 'cmake --install' is the AppImage input.
endif()

# ─────────────────────────────────────────────────────────────────────────────
include(CPack)
