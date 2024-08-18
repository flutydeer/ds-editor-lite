vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/jetbrains-docking-system
    REF fecb9d9fb7d17e58bc9502324beae8ac0eb8b5d5
    SHA512 30db0cd6c09a55840314384f3dfcbeea22fe7ee70ec24f2177655cf70d6441c116e841a34c505c6e29ff6f8c0a265950da313a4b977591a9794491e97320cdff
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/JetBrainsDockingSystem)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)