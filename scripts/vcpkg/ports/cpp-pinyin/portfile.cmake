vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/cpp-pinyin
        REF  a915e330d86d25ab908fea691ce53124e069a873
        SHA512 3b54cfc5847818e3128a4d4ad571bcd3631be0b97be38621beb7e0fc08c50a0ac8e361f5c68c31efe07b17be32620d00e35fb0809af9992676129de348a48eb2
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DCPP_PINYIN_BUILD_STATIC=FALSE
        -DCPP_PINYIN_BUILD_TESTS=FALSE
        -DVCPKG_DICT_DIR=${CURRENT_PACKAGES_DIR}/share/${PORT}
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)