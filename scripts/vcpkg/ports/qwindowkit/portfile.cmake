vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdware/qwindowkit
    REF 295230fdda3feb81d2e1b797eefcfadc405df742
    SHA512 36a7da3dc2e3bb90d76f3e2455195d6b2a69cff747380c3f87b42a2ff8015e599217442179f5b722ae7efbe41ad0e3562087928684dff45ad5c8bc6413150c26
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQWINDOWKIT_BUILD_STATIC=OFF
        -DQWINDOWKIT_BUILD_WIDGETS=ON
        -DQWINDOWKIT_BUILD_QUICK=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/QWindowKit)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/" RENAME copyright)