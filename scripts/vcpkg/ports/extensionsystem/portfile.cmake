vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/ExtensionSystem
    REF 32e24d9a23b1275f8b317825badacd5a869508aa
    SHA512 a142170ff3f1d18e6a9d4c40f29b4b8c5d2a90b501c3fea1b99fd872543b98245e1c9a6f4fc83edeb299aae0e13d4b10711a2afd1c0fb9bc45f813795209d96c
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/ExtensionSystem)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)