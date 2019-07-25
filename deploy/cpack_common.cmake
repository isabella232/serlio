# CPACK common variables, included by both cpack_archives.cmake and cpack_installer.cmake.
if(SRL_WINDOWS)
    set(SRL_PKG_OS "windows")
    set(SRL_INST_OS "win64")
elseif(SRL_MACOS)
    set(SRL_PKG_OS "macos")
    set(SRL_INST_OS "macOS")
elseif(SRL_LINUX)
    set(SRL_PKG_OS "linux")
    set(SRL_INST_OS "linux")
else()
    set(SRL_PKG_OS "unknown")
    set(SRL_INST_OS "unknown")
endif()

set(CPACK_PACKAGE_NAME                "serlio")
set(CPACK_PACKAGE_VENDOR              "Esri R&D Center Zurich")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Esri CityEngine Plugin for Autodesk Maya")
set(CPACK_PACKAGE_VERSION_MAJOR       ${SRL_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${SRL_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${SRL_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION             ${SRL_VERSION_MMP_PRE})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "serlio-${SRL_VERSION}")
set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${SRL_VERSION}-${SRL_PKG_OS}")

configure_file(${CMAKE_CURRENT_LIST_DIR}/deployment.properties.in deployment.properties @ONLY)

