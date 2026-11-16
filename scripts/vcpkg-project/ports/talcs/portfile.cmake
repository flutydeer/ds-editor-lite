vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO CrSjimo/talcs
    REF 2ad6028f48dc6dbfba4aae1a99eea8319aa6f4e9
    SHA512 91856e3f2446218841f8724f3db2a23eddf12284a4df50ce921bf5ef2798075004116b016dd45f8bc7a5d0e8df8c236da87ee65e63bdaff7cc7ef5788713c6e2
    PATCHES
        fix-loop-position.patch
        fix-clip-series-read-serialization.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        device TALCS_DEVICE
        asio TALCS_ASIO
        alsa TALCS_ALSA
        pulseaudio TALCS_PULSEAUDIO
        pipewire TALCS_PIPEWIRE
        jack TALCS_JACK
        format TALCS_FORMAT
        midi TALCS_MIDI
        remote TALCS_REMOTE
        dspx TALCS_DSPX
        gui TALCS_GUI
        widgets TALCS_WIDGETS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTALCS_DEVICE=${TALCS_DEVICE}
        -DTALCS_ASIO=${TALCS_ASIO}
        -DTALCS_ALSA=${TALCS_ALSA}
        -DTALCS_PULSEAUDIO=${TALCS_PULSEAUDIO}
        -DTALCS_PIPEWIRE=${TALCS_PIPEWIRE}
        -DTALCS_JACK=${TALCS_JACK}
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
