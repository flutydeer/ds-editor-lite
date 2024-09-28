vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/language-manager
        REF fa5a6ce6e81cd93d008291008e0bf97327fe41fc
        SHA512 0532901db4070c2d8817754bc045afc709e0c58abee2a3e464986aae0d637c54fc20a400dd8b6a6d63e1e4398871d95c9604fae5c8a4b9e3e9b1c1d5c4098d5d
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