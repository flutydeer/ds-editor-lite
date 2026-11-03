# Install selected wolf language resources as data packages.

set(VCPKG_POLICY_EMPTY_PACKAGE enabled)

include("${CMAKE_CURRENT_LIST_DIR}/assets.cmake")

set(WOLF_LANG_RELEASE_TAG "lang-v${WOLF_LANG_PACKAGES_BUNDLE_VERSION}")
set(WOLF_LANG_BASE_URL
    "https://github.com/diffscope/wolf/releases/download/${WOLF_LANG_RELEASE_TAG}")

set(WOLF_LANG_PACKAGES_SUBDIR "share/wolf/packages")
set(_install_root "${CURRENT_PACKAGES_DIR}/${WOLF_LANG_PACKAGES_SUBDIR}")
file(MAKE_DIRECTORY "${_install_root}")

set(_installed "")
foreach(_suite IN LISTS WOLF_LANG_PACKAGES_SUITES)
    if(NOT _suite IN_LIST FEATURES)
        continue()
    endif()

    string(TOUPPER "${_suite}" _key)
    if(NOT DEFINED WOLF_LANG_${_key}_SHA512)
        message(FATAL_ERROR "wolf-lang-packages: no asset recorded for feature ${_suite}")
    endif()
    if(_suite STREQUAL "multi" OR _suite STREQUAL "pinyin")
        set(_package "wolf-g2p-${_suite}")
    else()
        set(_package "wolf-lang-${_suite}")
    endif()
    set(_filename "${_package}-${WOLF_LANG_PACKAGES_BUNDLE_VERSION}.zip")

    vcpkg_download_distfile(_archive
        URLS "${WOLF_LANG_BASE_URL}/${_filename}"
        FILENAME "${_filename}"
        SHA512 "${WOLF_LANG_${_key}_SHA512}"
    )
    vcpkg_extract_source_archive(_extracted ARCHIVE "${_archive}" NO_REMOVE_ONE_LEVEL)
    file(COPY "${_extracted}/${_package}" DESTINATION "${_install_root}")
    list(APPEND _installed "${_suite}")
endforeach()

set(WOLF_LANG_PACKAGES_INSTALLED "${_installed}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/wolf-lang-packages-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/wolf-lang-packages-config.cmake" @ONLY)

# Collect licenses from the selected archives; some archives contain no license file.
string(REPLACE ";" ", " _installed_text "${_installed}")
set(_copyright "Language resources redistributed by the wolf project.\n")
string(APPEND _copyright "Installed packages: ${_installed_text}\n\n")
string(APPEND _copyright
       "Individual dictionaries carry their own license files where upstream supplied them.\n"
       "Those files are installed alongside the data and reproduced below.\n")
file(GLOB_RECURSE _licenses "${_install_root}/*/License.txt" "${_install_root}/*/LICENSE"
     "${_install_root}/*/COPYING")
foreach(_license IN LISTS _licenses)
    file(RELATIVE_PATH _where "${_install_root}" "${_license}")
    file(READ "${_license}" _text)
    string(APPEND _copyright "\n---- ${_where} ----\n${_text}\n")
endforeach()
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "${_copyright}")
