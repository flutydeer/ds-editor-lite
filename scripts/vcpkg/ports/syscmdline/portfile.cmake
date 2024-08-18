vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SineStriker/syscmdline
    REF 70e18ba18056bff1bebab924dde73dbbf04d46f9
    SHA512 e9ddce733fabe99d2459de9564c27fef578e28072d8d0dd5dce7edee84ecb53e34b597cb5404086201051f8c1fde756c104d890426a12ce8b859e33b14ea60a5
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)