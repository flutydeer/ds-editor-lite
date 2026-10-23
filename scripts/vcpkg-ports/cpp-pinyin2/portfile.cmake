# cpp-pinyin 2.x has a separate C ABI and CMake package and can coexist with 1.x.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfgitpr/cpp-pinyin
    REF 0d9a5656aba4c473abe7abc8332434a1e933ffa3
    SHA512 61abc9238ba3a182ebe998b8337ddb6fd9433f72f0c1a6491da8c223c1e708d8265ee37efbccb9bf0e419acbc8b472a30cc76b39bf148ea2fe1a6ba4592e470c
    HEAD_REF v2
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPP_PINYIN2_BUILD_TESTS=OFF
        -DCPP_PINYIN2_BUILD_EXAMPLES=OFF
        -DCPP_PINYIN2_INSTALL=ON
)

vcpkg_cmake_install()

# Move the package config to share/cpp-pinyin2 so its relative dictionary path resolves.
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/cpp-pinyin2" PACKAGE_NAME cpp-pinyin2)

vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# Install code and dictionary licenses separately.
vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/LICENSE"
        "${SOURCE_PATH}/THIRD_PARTY_NOTICES.md"
)

configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
