vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO diffscope/opendspx
        REF 24439c505166eac72a907444e1f389e17ae14a05
        SHA512 561dc1ef82a1cc6baa35452efd5d58b4831e40b60a39255640152f534f76e9d86faf30da0757830b62aa7322dd44a0e758990145848f053a0bbb594a0cf95154
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)