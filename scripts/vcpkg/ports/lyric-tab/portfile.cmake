vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/lyric-tab
        REF 88336418478f8ddbdad753b870ba8b3ffe3cc842
        SHA512 10d3bc550c2a05e1249ef6f75f23a2964d4337351b438e5b5403b427349ee7aa26162c064a27835d30c1bbe78365aee0c7b97043a48659579d2557540b112824
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