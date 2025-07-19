vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/qwindowkit
    REF ${VERSION}
    SHA512 35a1c5aa6ca8771196cba97c1c37704a3953b3892b4e309c8d637dfb78d5ff9af729e9273f80934a7231ab7fe813ed0433630f74077ae440ac0e84423382df86
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        widgets QWINDOWKIT_BUILD_WIDGETS
        quick QWINDOWKIT_BUILD_QUICK
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQWINDOWKIT_BUILD_STATIC=OFF
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/QWindowKit)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)