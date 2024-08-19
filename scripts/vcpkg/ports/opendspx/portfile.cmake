vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO diffscope/opendspx
        REF e838780a69ace74912d251a072584f4e8ac64b7a
        SHA512 cb8c0c951a08b48191a82adf122ed71555e691417e37d615b6662d95380825e129199b4bcecd932c1ae65dbf77856d2e87929117d37b3b5412814b000162f41b
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