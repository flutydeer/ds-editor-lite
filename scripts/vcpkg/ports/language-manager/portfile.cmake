vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/language-manager
        REF 6dd476cedfdc6824dc4fdf34103d3b61498d939c
        SHA512 67d7a25baaadff1a81cef5237cfbbb3a2b6ba660572f1afd968d093da6ed1e7b38a3d61180d14c7e1f502915a8b4369b61b7b397913b082561c032da3d18e583
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