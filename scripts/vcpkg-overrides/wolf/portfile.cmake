# wolf, pinned to the commit this editor was built and tested against.
#
# Depending on wolf is the only way a consumer gets its contribution category registered: linking
# the library is what runs the registration before main, so vendoring headers would not do.

# A fetch by commit needs no archive hash.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/wolf.git
    REF d96e14355485c5202159a6195668d50a0b0446cf
    HEAD_REF linguistic-level-1
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWOLF_BUILD_TESTS:BOOL=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME wolf CONFIG_PATH lib/cmake/wolf)

# The plugins install under lib/plugins/wolf/<category>/<name>/, which is a search path the host hands to SynthUnit rather than a link target, so vcpkg's usual expectations about lib/ do not apply to them.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
