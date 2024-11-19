vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfgitpr/cpp-pinyin
    REF  ebbc4921d7bf9f821366bd2d882e435dc41819fd
    SHA512 84b95a67af2a83cae1e958077a7ae84ea09e1740d836f135e93e428a87d3b5789e15e29b12743436068dd1490687f0262b5e57188df4b39d48a19c61b3fea618
    HEAD_REF main
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" CPP_PINYIN_BUILD_STATIC)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPP_PINYIN_BUILD_STATIC=${CPP_PINYIN_BUILD_STATIC}
        -DCPP_PINYIN_BUILD_TESTS=FALSE
        "-DVCPKG_DICT_DIR=${CURRENT_PACKAGES_DIR}/share/${PORT}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
