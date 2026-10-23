# cpp-pinyin2 -- the 2.x line of cpp-pinyin (C ABI cppp2_*, package/target cpp-pinyin2).
#
# It is a NEW port rather than a version bump of `cpp-pinyin`: 2.x renamed every public entity, so a
# bump would break every existing `find_package(cpp-pinyin)` consumer. Both ports install side by
# side; the 1.x one stays frozen at 1.0.2.
#
# Pinned to branch `v2` as of commit 0d9a565 rather than to a release tag: no `v2.0.0` tag exists yet.
# A commit, not the branch name, is the ref, because a branch ref would silently change what this port
# builds; when upstream publishes a newer commit, change REF and re-stamp SHA512 with the archive of
# that commit.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfgitpr/cpp-pinyin
    REF 0d9a5656aba4c473abe7abc8332434a1e933ffa3
    SHA512 61abc9238ba3a182ebe998b8337ddb6fd9433f72f0c1a6491da8c223c1e708d8265ee37efbccb9bf0e419acbc8b472a30cc76b39bf148ea2fe1a6ba4592e470c
    HEAD_REF v2
)

# Passed explicitly even though the project defaults are already the ones wanted: a consumer's
# triplet must not depend on someone else's defaults staying what they are.
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCPP_PINYIN2_BUILD_TESTS=OFF
        -DCPP_PINYIN2_BUILD_EXAMPLES=OFF
        -DCPP_PINYIN2_INSTALL=ON
)

vcpkg_cmake_install()

# The package config is installed to lib/cmake/cpp-pinyin2 and must be moved to share/cpp-pinyin2,
# which is also what makes ${cpp-pinyin2_DIR}/dict resolve to the tables this port installs.
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/cpp-pinyin2" PACKAGE_NAME cpp-pinyin2)

vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# Two files, because the license is split and a redistributor has to know which half is which: the
# code is Apache-2.0, the dictionary tables ship under CC BY-SA (4.0 Mandarin, 3.0 Cantonese).
vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/LICENSE"
        "${SOURCE_PATH}/THIRD_PARTY_NOTICES.md"
)

configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
