# Packaging layout check for the wolf language packages staged by a build.
#
# The script runs as the last POST_BUILD command on the application target, after the copy commands
# that it verifies. It receives the destination directory of those copies (the land point) and the
# package names resolved at configure time. It rejects a tree in which the runtime would find no
# package: an absent land point, a package directory without desc.json, a missing package that this
# build staged, or an entry that is not a directory.
#
# No other step detects these failures. A build that staged no package resolves no language at run
# time, and the qmsetup copy script (modules/scripts/copy.cmake) does not fail if a source directory
# globbed at configure time is removed before the build: it copies an empty set and exits with 0, so
# the build would report success without staging any package.
#
# If no package tree was resolved at configure time, nothing is staged. A developer tree is allowed
# in that state and the build does not fail, but the build output reports it: the configure-time
# warning is easily missed, and the resulting runtime resolves no wolf/lang-* dependency.
#
# The install tree does not exist during the build. The script therefore checks the install rules
# generated at configure time for the same land point below the install prefix. If LITE_INSTALL is
# off or no install script exists, the script reports that the install tree is not checked.
#
# Usage from cmake/LiteBuildApi.cmake, after the copy commands:
#
#   cmake -D "LITE_STAGED_LANG_PACKAGES_DIR=<build runtime dir>/wolf/packages" ^
#         -D "expected=wolf-g2p-pinyin,wolf-lang-cmn" -D "land_point=wolf/packages" ^
#         -D "source=<the package tree this build resolved>" ^
#         -D "problem=<reason no package tree was resolved, if none was>" ^
#         -D "hint=<how to fix it>" -D "install_enabled=ON" ^
#         -D "install_script=<build>/cmake_install.cmake" ^
#         -P cmake/LitePackagingLayoutCheck.cmake
#
# Every finding in this file is reported through the two functions below, so that the severity
# depends only on the layout: FATAL_ERROR by default, and WARNING under
# LITE_INSTALL_DIR_USE_DEBUG_PREFIX. The packaging scripts do not support that layout because it
# moves the application below <prefix>/debug but not the copies. A tree built with that layout
# cannot be packaged, but building it is not an error.
function(_lite_packaging_severity_rule _out)
    if(LITE_INSTALL_DIR_USE_DEBUG_PREFIX)
        set(${_out} WARNING PARENT_SCOPE)
    else()
        set(${_out} FATAL_ERROR PARENT_SCOPE)
    endif()
endfunction()

# Reports a finding with the given severity. No other code in this file issues its own
# message(WARNING) for a missing path, so a severity cannot be lowered by accident.
function(_lite_packaging_verdict _severity _message)
    if(_severity STREQUAL "FATAL_ERROR")
        message(FATAL_ERROR "${_message}")
    else()
        message(WARNING "${_message}")
    endif()
endfunction()

# ------------------------------------------------------------------------------------------------
# Staged build tree
#
# Verification of the land point in the build tree against the package names resolved at configure
# time (see the file header).
if(DEFINED LITE_STAGED_LANG_PACKAGES_DIR)
    _lite_packaging_severity_rule(_severity)
    set(_stage_dir "${LITE_STAGED_LANG_PACKAGES_DIR}")

    # Classification of the entries in the land point. A package directory is a directory that
    # contains desc.json, because the loader scans this directory instead of reading a list of
    # names; every other entry resolves to no package.
    file(GLOB _stage_entries "${_stage_dir}/*")
    set(_staged "")
    set(_staged_without_manifest "")
    set(_staged_not_directories "")
    foreach(_entry IN LISTS _stage_entries)
        get_filename_component(_entry_name "${_entry}" NAME)
        if(NOT IS_DIRECTORY "${_entry}")
            list(APPEND _staged_not_directories "${_entry_name}")
        elseif(EXISTS "${_entry}/desc.json")
            list(APPEND _staged "${_entry_name}")
        else()
            list(APPEND _staged_without_manifest "${_entry_name}")
        endif()
    endforeach()

    if(NOT source)
        set(source "(no source named)")
    endif()
    if(NOT hint)
        # Built with string(CONCAT) instead of set() with one argument per line: set() would join
        # the arguments with ';', which would appear in the printed text.
        string(CONCAT hint
            "Set LITE_WOLF_LANG_PACKAGES, or the WOLF_LANG_PACKAGES_SOURCE environment variable, "
            "to the directory of unpacked packages.")
    endif()

    if(NOT expected)
        # No package tree was resolved for this build. The warnings report the consequence and
        # any packages from an earlier configure that remain in the tree and mask the failure.
        if(NOT problem)
            set(problem "no package tree was resolved at configure time")
        endif()
        string(CONCAT _message
            "[lang-packages] No wolf language package is staged into '${_stage_dir}': ${problem}. "
            "The runtime resolves no wolf/lang-* dependency, so no package that requires such a "
            "dependency loads. "
            "${hint}")
        message(WARNING "${_message}")
        if(_staged)
            list(LENGTH _staged _stale_count)
            string(REPLACE ";" ", " _stale_text "${_staged}")
            message(WARNING
                "[lang-packages] ${_stage_dir} still contains ${_stale_count} package "
                "directory(ies) (${_stale_text}) from an earlier configure. The current configure "
                "stages none of them, so their presence does not indicate that the packages "
                "required by this build are present.")
        endif()
        return()
    endif()

    # Package names that this build stages. The caller separates the names with ',' because a
    # ';'-separated CMake list would reach this script as one argument per package.
    string(REPLACE "," ";" _expected_names "${expected}")
    list(LENGTH _expected_names _expected_count)
    list(LENGTH _staged _staged_count)

    set(_missing "")
    foreach(_name IN LISTS _expected_names)
        # list(FIND) instead of IN_LIST: this file also runs in script mode (-P), in which no
        # project() has run and the policy that enables the IN_LIST operator (CMP0057) is not in
        # effect.
        list(FIND _staged "${_name}" _staged_at)
        if(_staged_at EQUAL -1)
            list(APPEND _missing "${_name}")
        endif()
    endforeach()
    set(_extra "")
    foreach(_name IN LISTS _staged)
        list(FIND _expected_names "${_name}" _expected_at)
        if(_expected_at EQUAL -1)
            list(APPEND _extra "${_name}")
        endif()
    endforeach()

    set(_findings "")
    if(NOT IS_DIRECTORY "${_stage_dir}")
        list(APPEND _findings "there is no such directory")
    else()
        if(_missing)
            list(APPEND _findings "it does not contain ${_missing}")
        elseif(NOT _staged)
            list(APPEND _findings "no subdirectory contains a desc.json")
        endif()
        if(_staged_without_manifest)
            list(APPEND _findings
                "these directories contain no desc.json: ${_staged_without_manifest}")
        endif()
        if(_staged_not_directories)
            list(APPEND _findings "these entries are not directories: ${_staged_not_directories}")
        endif()
    endif()

    if(_findings)
        string(REPLACE ";" ", " _findings_text "${_findings}")
        string(CONCAT _message
            "[lang-packages] This build stages ${_expected_count} package(s) from '${source}' into "
            "'${_stage_dir}', but the tree does not contain them: ${_findings_text}. Expected one "
            "subdirectory per package, each containing its desc.json; the tree contains "
            "${_staged_count} package directory(ies). ${hint} Then rebuild.")
        _lite_packaging_verdict("${_severity}" "${_message}")
        return()
    endif()

    if(_extra)
        string(CONCAT _extra_note
            " It also contains ${_extra}, which this configure did not stage. These directories "
            "remain from an earlier source and do not indicate what this build deployed.")
    else()
        set(_extra_note "")
    endif()
    message(STATUS
        "[lang-packages] Staged ${_staged_count} wolf language package(s) from '${source}' into "
        "'${_stage_dir}', each with a desc.json.${_extra_note}")

    # The install tree does not exist yet. The check therefore reads the install rules generated at
    # configure time and verifies that they install the same packages to the same land point below
    # the install prefix. The qmsetup install copies report their own failures, but only for
    # registered copies; a package absent from the install rules is silently not installed.
    if(NOT install_enabled)
        message(STATUS
            "[lang-packages] No install rules were requested (LITE_INSTALL is off, or the caller "
            "passed no install_enabled): '${land_point}' in the install tree is not checked.")
    elseif(NOT land_point)
        message(FATAL_ERROR
            "[lang-packages] install_enabled is set but land_point is empty; pass the package "
            "root below the install prefix, for example wolf/packages.")
    elseif(NOT EXISTS "${install_script}")
        message(STATUS
            "[lang-packages] No install script at '${install_script}': the install rules cannot be "
            "read, so '${land_point}' in the install tree is not checked.")
    else()
        file(READ "${install_script}" _install_text)
        set(_uninstalled "")
        foreach(_name IN LISTS _expected_names)
            string(FIND "${_install_text}" "${land_point}/${_name}" _at)
            if(_at EQUAL -1)
                list(APPEND _uninstalled "${_name}")
            endif()
        endforeach()
        if(_uninstalled)
            string(REPLACE ";" ", " _uninstalled_text "${_uninstalled}")
            string(CONCAT _message
                "[lang-packages] The install rules do not stage ${_uninstalled_text} into the "
                "install prefix's '${land_point}': the generated install script "
                "('${install_script}') contains no '${land_point}/<package>' path. An installed "
                "tree would resolve no wolf/lang-* dependency provided by those packages. Fix: the "
                "copy commands below the deployment root in cmake/LiteBuildApi.cmake register "
                "these install rules if LITE_INSTALL is on.")
            _lite_packaging_verdict("${_severity}" "${_message}")
        else()
            message(STATUS
                "[lang-packages] The install rules stage the same ${_expected_count} package(s) "
                "into the install prefix's '${land_point}'.")
        endif()
    endif()
    return()
endif()
