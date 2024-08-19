vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/cpp-kana
        REF 52efec85c4d6709444da0f74295c8f2fbf9467f7
        SHA512 44c0ba6d7a7f43471b24936fbdc31c84e07345cbbc8a9a9daefd4119daee4396cc4d5f8fbd2380e99368d454bc3da1a0fdf2eadfa5621f01069d857e25da2e31
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DCPP_KANA_BUILD_STATIC=FALSE
        -DCPP_KANA_BUILD_TESTS=FALSE
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)