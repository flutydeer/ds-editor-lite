vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfgitpr/cpp-pinyin
    REF  61e1cbceadb3e5312dc60342ecf370f3dbf853f9
    SHA512 152d675a0ef4494f623225e28eef07e63e4bea75f52acfb26b8572596e9a76b01a784ac673aa2c221bcbb9df92d2c775af944f433f0f6f653995fef6d4eb3a2c
    HEAD_REF dev
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" CPP_PINYIN_BUILD_STATIC)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPP_PINYIN_BUILD_STATIC=${CPP_PINYIN_BUILD_STATIC}
        -DCPP_PINYIN_BUILD_TESTS=FALSE
        -DCPP_PINYIN_VCPKG_DICT_DIR=${CURRENT_PACKAGES_DIR}/share/${PORT}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
