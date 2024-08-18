vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/cpp-pinyin
        REF  48181d0c7d9453a9787bdf4a180cc0394bf7d04b
        SHA512 d31660ec713aa81758f7eaf521e993efe0ead0517e838600f03b652640881e0fc830d8bb6528e1a9d5a5973fbf3d577727b88e4bc4a1632ae132e66aacf0cbdf
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DCPP_PINYIN_BUILD_STATIC=FALSE
        -DCPP_PINYIN_BUILD_TESTS=OFF
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)