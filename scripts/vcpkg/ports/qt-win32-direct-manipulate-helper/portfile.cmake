vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CrSjimo/QtWin32DirectManipulateHelper
    REF be9f7ec24052010b6a47f284a7f281db70ea90aa
    SHA512 d7cc50ad4f5d1d8770585aba70c22d854a7d112a0b0d928ec2cec1cd00b93ca01c73f064b1ddb55585c7a519d6f094ba70c499fa534e2a22d25afaf27635170c
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQWDMH_BUILD_STATIC=$$<IF:$$<STREQUAL:${VCPKG_LIBRARY_LINKAGE},static>,ON,OFF>
        -DQWDMH_BUILD_EXAMPLES=OFF
        -DQWDMH_INSTALL=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/QtWin32DirectManipulateHelper)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
