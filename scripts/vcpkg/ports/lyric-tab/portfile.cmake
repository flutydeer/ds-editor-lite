vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/lyric-tab
        REF 02cbb474f846d2cc685599cc48bc1e84456b8ee1
        SHA512 0d735c9c6fa4f387019c17a799a6556a82cd8c8ce7fd541123770bc69a9376b737ffe31b5d3cf9a3ad55e24841c8af6b4ef41be4bea915ddc430c11329cb4dcf
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