# A copy of wolf's scripts/vcpkg-ports/wolf-lang-packages, kept in this repository for the same
# reason as the other ports in this directory: the editor's manifest must resolve from this
# repository alone. wolf's make-lang-release.py generates assets.cmake. A new release is adopted by
# copying the directory again.
#
# The port installs data only and compiles nothing. Each selected feature names one archive from a
# wolf release, which is downloaded, checked against its SHA512, and unpacked into a Package search
# directory.
#
# The release must be reachable without credentials. This port deliberately contains no token
# handling: a credential belongs in neither a committed file nor the environment of every consumer
# that builds the port, and a data-only port that any consumer can install must not hold a
# credential. A consumer without network access instead sets the WOLF_LANG_PACKAGES_SOURCE cache
# variable to the path of an unpacked local copy.
#
# The archives unpack to directories rather than to single files because the loader in synthrt
# main accepts directories only.

set(VCPKG_POLICY_EMPTY_PACKAGE enabled)

include("${CMAKE_CURRENT_LIST_DIR}/assets.cmake")

# The release tag is derived from the bundle version, so the assets in this port and the release
# that serves them must change together. If the bundle version changes and no release is published
# under the new tag, every feature points at a URL that does not exist. Conversely,
# make-lang-release.py rejects a rewrite of assets.cmake if the archives change and the bundle
# version does not.
set(WOLF_LANG_RELEASE_TAG "lang-v${WOLF_LANG_PACKAGES_BUNDLE_VERSION}")
set(WOLF_LANG_BASE_URL
    "https://github.com/diffscope/wolf/releases/download/${WOLF_LANG_RELEASE_TAG}")

set(_install_root "${CURRENT_PACKAGES_DIR}/share/wolf/packages")
file(MAKE_DIRECTORY "${_install_root}")

set(_installed "")
foreach(_suite IN LISTS WOLF_LANG_PACKAGES_SUITES)
    if(NOT _suite IN_LIST FEATURES)
        continue()
    endif()

    string(TOUPPER "${_suite}" _key)
    string(REPLACE "-" "_" _key "${_key}")
    if(NOT DEFINED WOLF_LANG_${_key}_FILE)
        message(FATAL_ERROR "wolf-lang-packages: no asset recorded for feature ${_suite}")
    endif()

    vcpkg_download_distfile(_archive
        URLS "${WOLF_LANG_BASE_URL}/${WOLF_LANG_${_key}_FILE}"
        FILENAME "${WOLF_LANG_${_key}_FILE}"
        SHA512 "${WOLF_LANG_${_key}_SHA512}"
    )
    vcpkg_extract_source_archive(_extracted ARCHIVE "${_archive}" NO_REMOVE_ONE_LEVEL)
    file(COPY "${_extracted}/${WOLF_LANG_${_key}_DIR}" DESTINATION "${_install_root}")
    list(APPEND _installed "${_suite}")
endforeach()

set(WOLF_LANG_PACKAGES_INSTALLED "${_installed}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/wolf-lang-packages-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/wolf-lang-packages-config.cmake" @ONLY)

# The copyright file is written directly rather than through vcpkg_install_copyright, which
# requires FILE_LIST to name at least one file. No fixed file list applies: the applicable licenses
# depend on the selected features, and several packages contain no license file because upstream
# supplied none. The license files that the installed packages contain are therefore collected,
# which records the licensing more completely than a fixed list.
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
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" ${_copyright})
