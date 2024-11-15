vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CrSjimo/talcs
    REF v0.1.0
    SHA512 d56df12cd3a3c859c991838c18680e2537a5c7b63b5dc63b451325ce668ed8c3fdcf8563eddd292993ea531a8720094211de0a88d28f89227e86da7ecd47c681
)

vcpkg_from_github(
    OUT_SOURCE_PATH R8BRAIN_PATH
    REPO avaneev/r8brain-free-src
    REF 3c930bf6825c0cfea4a813210c83b1a650c820b5
    SHA512 e4f6a6f493f13e0ddbba063a7afd066cd3ae07153898ba3b525236f1d3bc4fcfd4538e8314fcb2a9514abf4af903ec184141dfbe928f282ea7f991ceb5505fb5
)

file(INSTALL ${R8BRAIN_PATH} DESTINATION ${SOURCE_PATH}/lib/r8brain RENAME r8brain-free-src)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        device TALCS_DEVICE
        device-sdl TALCS_DEVICE_ENABLE_SDL
        device-asio TALCS_DEVICE_ENABLE_ASIO
        format TALCS_FORMAT
        midi TALCS_MIDI
        remote TALCS_REMOTE
        dspx TALCS_DSPX
        gui TALCS_GUI
        widgets TALCS_WIDGETS
)

set(TALCS_ASIOSDK_DIR)
if(TALCS_DEVICE_ENABLE_ASIO)
    vcpkg_cmake_configure(
        SOURCE_PATH ${CMAKE_CURRENT_LIST_DIR}/asiosdk_dir_helper
        OPTIONS
            -DASIOSDK_DIR_HELPER_OUT_FILE=${CURRENT_BUILDTREES_DIR}/asiosdk_dir
    )
    file(READ ${CURRENT_BUILDTREES_DIR}/asiosdk_dir TALCS_ASIOSDK_DIR)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTALCS_DEVICE=${TALCS_DEVICE}
        -DTALCS_DEVICE_ENABLE_SDL=${TALCS_DEVICE_ENABLE_SDL}
        -DTALCS_DEVICE_ENABLE_ASIO=${TALCS_DEVICE_ENABLE_ASIO}
        -DTALCS_ASIOSDK_DIR=${TALCS_ASIOSDK_DIR}
        -DTALCS_FORMAT=${TALCS_FORMAT}
        -DTALCS_MIDI=${TALCS_MIDI}
        -DTALCS_REMOTE=${TALCS_REMOTE}
        -DTALCS_DSPX=${TALCS_DSPX}
        -DTALCS_GUI=${TALCS_GUI}
        -DTALCS_WIDGETS=${TALCS_WIDGETS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ${PORT} CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING" "${SOURCE_PATH}/COPYING.FDL" "${SOURCE_PATH}/COPYING.LESSER")