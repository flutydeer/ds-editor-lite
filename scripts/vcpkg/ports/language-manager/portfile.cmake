vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/language-manager
        REF 21551d9301cb5f0a18a92e84b01dbe9558f47c7b
        SHA512 98dcdf176484c682f87f5755b4c1bdc100c5176f1ff1f85249bacb7208ede20d3c050197bd0bf5f9591dbbb3f36370dc555a4a6190014eb5eca0bbac749f7c06
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