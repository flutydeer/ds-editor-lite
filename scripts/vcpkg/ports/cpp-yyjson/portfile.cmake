vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yosh-matsuda/cpp-yyjson
    REF 682c32b7cbf16e1d999912fba6c25b2860fe67b5
    SHA512 4d165804c34b244047714aa171ed27f6184f04c786cd4c71480d46b017089f97724059d93742cc70dfea6f383678f909c733a0c5b0ba79cd26b1fcf5124c8825
    PATCHES
        remove-fmt.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPPYYJSON_BUILD_TEST=FALSE
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME cpp_yyjson CONFIG_PATH lib/cmake/cpp_yyjson)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)