vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/lyric-tab
        REF 92a9e60c179dbe7c369d186c6b92b9704680de94
        SHA512 49d49d9ed971e0d940ba4c889bc3798be05e2a60eae0ba4025b5ff85a536a59ec5f82f8e385e6eab4da63171d917293d93a78a507358f2d8a6b59261663bffe4
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DLYRIC_TAB_BUILD_STATIC=FALSE
        -DLYRIC_TAB_BUILD_TESTS=FALSE
        -DUSE_LITE_CONTROLS=FALSE
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)