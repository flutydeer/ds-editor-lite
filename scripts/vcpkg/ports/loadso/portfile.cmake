vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SineStriker/loadso
    REF 141896a0a792e6b0a4c858aec2e2f7de4f631762
    SHA512 981ac320b0201e13030065dbaef98b08fdd7ac82331abe95c79b49c6cc82c41074409c3d9cab1303edd291aa2800485a01b899c260025f8cc6215d6e397b99ab
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)