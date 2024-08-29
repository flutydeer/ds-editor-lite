vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO wolfgitpr/cpp-pinyin
        REF  9c5bd3aeb73bc928fd848acabc5727949be5ce9e
        SHA512 121094b60c70012a0e79b2fe214048e23d98f9328ea2a435696e2c56c882747606ab20bc629a087a69a450c4093fe3c93e5906e1674661107229cb96509cb79b
        HEAD_REF main
)

vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
        -DCPP_PINYIN_BUILD_STATIC=FALSE
        -DCPP_PINYIN_BUILD_TESTS=FALSE
        -DVCPKG_DICT_DIR=${CURRENT_PACKAGES_DIR}/share/${PORT}
)

vcpkg_cmake_install()

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/share/${PORT}")

vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)