vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/qwindowkit
    REF f93657fa82bdd37dba68ea962d5e9b2cf4fd4d60
    SHA512 5ced0f8e2cb074924bc7ed37449f377a66e09681b7ccd627ad8403a142db0213b80893f5a20c8f7243eb2238eae6b3da799b8ec010b35b7d626608caf0f86d97
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQWINDOWKIT_BUILD_STATIC=OFF
        -DQWINDOWKIT_BUILD_WIDGETS=ON
        -DQWINDOWKIT_BUILD_QUICK=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/QWindowKit)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)