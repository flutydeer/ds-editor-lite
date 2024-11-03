vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/language-manager
        REF 7a028ac7fbb2fe6df265b6e4ee1000f57192afdf
        SHA512 db4f3963a14310fe5e19214d4fed0e4cbeb1f8aeec91a1be37db9c6e8bac69d6bf0e5b0db5033115d0aea378cddf0bc444514063edefea6b621662d5c6030012
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