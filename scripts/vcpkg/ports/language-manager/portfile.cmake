vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/language-manager
        REF c92ae31cc5428e5ff8cf1ee702ae2309ab57b640
        SHA512 b9fa15bb7a87c4586746b16dd376f7393179d7f5dfc1c4b892a3276c264454bfbfff2f916b11a15e9da436e298406dbd9349af18d7c6117b285454fae638960b
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DLANG_MANAGER_BUILD_STATIC=FALSE
        -DLANG_MANAGER_BUILD_TESTS=FALSE
        -DLANG_VCPKG_INSTALL=TRUE
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)