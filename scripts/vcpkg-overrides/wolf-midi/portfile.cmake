vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfgitpr/wolf-midi
    REF ad6e015fd288535eaad1a7a394628b0f5d8d5cd9
    SHA512 b8ba2050473e402432f4a9743a9d0a93ee3136b67314afeee026704d026ec5c181d30ac6e7f762b5888f8b5a1b5c8548a9433da14adbff40e6d5d2019c7cdc89
    HEAD_REF main
    PATCHES include-cmath.patch
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" WOLF_MIDI_BUILD_STATIC)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS -DWOLF_MIDI_BUILD_STATIC=${WOLF_MIDI_BUILD_STATIC} -DWOLF_MIDI_BUILD_TESTS=FALSE
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../vcpkg/ports/wolf-midi/usage"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
