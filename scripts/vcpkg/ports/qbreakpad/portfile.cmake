vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/qBreakpad
    REF 0.2.1.1
    SHA512 4d2065ded6b0b56ff9f1197753dcfc7ddcb6a6884eaa66ff4acf276ed893526495f4a8b083de6f98f1d32f69926866f09749202b546befd0e54e39ac7000da61
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQBREAKPAD_BUILD_STATIC=TRUE
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/qBreakpad)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE.LGPL" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)