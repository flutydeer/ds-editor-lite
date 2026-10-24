# Check the staged language packages and their install rules after deployment.
# Use warnings for the development-only debug-prefix layout, errors otherwise.

function(_lite_packaging_severity_rule _out)
    if(LITE_INSTALL_DIR_USE_DEBUG_PREFIX)
        set(${_out} WARNING PARENT_SCOPE)
    else()
        set(${_out} FATAL_ERROR PARENT_SCOPE)
    endif()
endfunction()

function(_lite_packaging_verdict _severity _message)
    if(_severity STREQUAL "FATAL_ERROR")
        message(FATAL_ERROR "${_message}")
    else()
        message(WARNING "${_message}")
    endif()
endfunction()

# Staged build tree
if(DEFINED LITE_STAGED_LANG_PACKAGES_DIR)
    _lite_packaging_severity_rule(_severity)
    set(_stage_dir "${LITE_STAGED_LANG_PACKAGES_DIR}")

    # Replace changed package versions: fixed archive timestamps defeat timestamp-based copying.
    if(expected AND source AND IS_DIRECTORY "${source}")
        string(REPLACE "," ";" _expected_names "${expected}")
        foreach(_name IN LISTS _expected_names)
            set(_source_manifest "${source}/${_name}/desc.json")
            if(NOT EXISTS "${_source_manifest}")
                continue()
            endif()
            file(READ "${_source_manifest}" _source_text)
            string(REGEX MATCH "\"version\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _source_match "${_source_text}")
            set(_source_version "${CMAKE_MATCH_1}")
            set(_staged_version "")
            set(_staged_manifest "${_stage_dir}/${_name}/desc.json")
            if(EXISTS "${_staged_manifest}")
                file(READ "${_staged_manifest}" _staged_text)
                string(REGEX MATCH "\"version\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _staged_match "${_staged_text}")
                set(_staged_version "${CMAKE_MATCH_1}")
            endif()
            if(NOT _staged_version STREQUAL _source_version)
                if(_staged_version STREQUAL "")
                    set(_staged_version "absent")
                endif()
                message(STATUS
                    "[lang-packages] Replacing ${_name} in '${_stage_dir}': staged "
                    "${_staged_version}, source ${_source_version}")
                file(REMOVE_RECURSE "${_stage_dir}/${_name}")
                file(COPY "${source}/${_name}/" DESTINATION "${_stage_dir}/${_name}")
            endif()
        endforeach()
    endif()

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
        string(CONCAT hint
            "Set LITE_WOLF_LANG_PACKAGES, or the WOLF_LANG_PACKAGES_SOURCE environment variable, "
            "to the directory of unpacked packages.")
    endif()

    if(NOT expected)
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

    string(REPLACE "," ";" _expected_names "${expected}")
    list(LENGTH _expected_names _expected_count)
    list(LENGTH _staged _staged_count)

    set(_missing "")
    foreach(_name IN LISTS _expected_names)
        # Use list(FIND) in script mode, where CMP0057 may not enable IN_LIST.
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
        if(_extra)
            list(APPEND _findings "unexpected package directories remain from an earlier source: ${_extra}")
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

    message(STATUS
        "[lang-packages] Staged ${_staged_count} wolf language package(s) from '${source}' into "
        "'${_stage_dir}', each with a desc.json.")

    # Validate generated install rules because the install tree does not exist during the build.
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
