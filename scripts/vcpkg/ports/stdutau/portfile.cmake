vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO diffscope/stdutau
    REF 7ffb254479b7509bc323c0b7a75739a61157de9b
    SHA512 814e952213d12177e4dcda456c8db2ccf5c23de9162489d0b76d8d8a09fede33a1647f243f7d24d1d8a8071002c8184dbf0856ec7caf7fe195301455a65219f3
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSTDUTAU_BUILD_STATIC=FALSE
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)