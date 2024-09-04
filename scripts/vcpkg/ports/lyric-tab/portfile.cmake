vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/lyric-tab
        REF b3ccbdaeeec457ed543296d3dc143f12b97ae97f
        SHA512 c787842ad5dbf81ca03db77776982791dc2493bbd192fbdde049be48552e59cb8f725eccc4be235584568b458a9c506092afd29e71609a939bf83c2a933ec578
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