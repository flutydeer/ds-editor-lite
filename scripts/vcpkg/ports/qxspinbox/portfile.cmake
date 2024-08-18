vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CrSjimo/QXSpinBox
    REF 16ae5c31abfe3bd21475a42a7bd37146dc12f9fc
    SHA512 d0b920cc6116deceeb55d79b71734c539748d1a0063d345507417b2466c4f509725dbc026a045ead40e8e6ca2c0b1bf6ede91bf3138afd0f1a8a900cc5edf030
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQXSPINBOX_BUILD_STATIC=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/QXSpinBox)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)