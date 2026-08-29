include_guard(DIRECTORY)

function(lite_add_test _target)
    lite_add_executable(${_target}
        TEST
        QT_AUTOGEN
        NO_INSTALL
        ${ARGN}
    )
endfunction()

function(lite_add_tool _target)
    lite_add_executable(${_target}
        QT_AUTOGEN
        NO_INSTALL
        ${ARGN}
    )
endfunction()

#[[
    Locate the platform's Qt deployment tool.

    _lite_find_qt_deploy_tool(<out-var>)

    Sets <out-var> to the windeployqt/macdeployqt path, or an empty string when it
    cannot be found. Qt plugins (platforms, imageformats, styles) and translations
    are deployed by the official Qt deployment tool.
]] #
function(_lite_find_qt_deploy_tool _out)
    set(${_out} "" PARENT_SCOPE)

    if(NOT WIN32 AND NOT APPLE)
        return()
    endif()

    set(_qmake "${QT_QMAKE_EXECUTABLE}")

    if(NOT _qmake AND TARGET Qt${QT_VERSION_MAJOR}::qmake)
        get_target_property(_qmake Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    endif()

    if(NOT EXISTS "${_qmake}")
        message(WARNING "lite_deploy_application: can't locate qmake, skipping Qt deployment.")
        return()
    endif()

    cmake_path(GET _qmake PARENT_PATH _qt_bin_dir)
    find_program(LITE_QT_DEPLOY_EXECUTABLE
        NAMES windeployqt macdeployqt
        HINTS "${_qt_bin_dir}"
    )

    if(NOT LITE_QT_DEPLOY_EXECUTABLE)
        message(WARNING "lite_deploy_application: can't locate the deployqt tool, "
            "skipping Qt deployment.")
        return()
    endif()

    set(${_out} "${LITE_QT_DEPLOY_EXECUTABLE}" PARENT_SCOPE)
endfunction()

function(lite_deploy_application _target)
    qm_import(Filesystem)

    _lite_find_qt_deploy_tool(_deploy_tool)

    if(LITE_INSTALL)
        set(_install_copy_args INSTALL_DIR .)
    else()
        set(_install_copy_args SKIP_INSTALL)
    endif()

    # Deploy the Qt runtime first: on macOS the plugins copied below must land inside a
    # bundle macdeployqt has already processed.
    if(_deploy_tool AND WIN32)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${_deploy_tool}"
                --verbose 0
                --translations zh_CN
                --plugindir "$<TARGET_FILE_DIR:${_target}>/plugins"
                --no-system-d3d-compiler
                --no-compiler-runtime
                --no-opengl-sw
                --pdb # Also deploy the Qt modules' .pdb files
                "$<TARGET_FILE:${_target}>"
            COMMENT "Deploy Qt"
        )
    elseif(_deploy_tool AND APPLE)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${_deploy_tool}"
                "$<TARGET_BUNDLE_DIR:${_target}>"
                -verbose=0
                -always-overwrite
            COMMENT "Deploy Qt"
        )
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_macos_dylib_paths.sh
                "$<TARGET_BUNDLE_DIR:${_target}>" "1"
            COMMENT "Fix dylib paths"
        )
    endif()

    if(APPLE)
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/Resources
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/FillLyric/configs/
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/MacOS/configs
            ${_install_copy_args}
        )
    else()
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
            DESTINATION Resources
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/FillLyric/configs/
            DESTINATION configs
            ${_install_copy_args}
        )
    endif()

    set(_g2p_packages
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/synthrt/G2pPackages)

    if(WIN32)
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE:cpp-pinyin::cpp-pinyin>
            DESTINATION .
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins/
            DESTINATION plugins
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES ${_g2p_packages}/
            DESTINATION plugins/srt-g2p/G2pPackages
            ${_install_copy_args}
        )
    elseif(APPLE)
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins/
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES ${_g2p_packages}/
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/srt-g2p/G2pPackages
            ${_install_copy_args}
        )
    elseif(UNIX)
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins/
            DESTINATION $<TARGET_FILE_DIR:${_target}>/../lib/plugins
            ${_install_copy_args}
        )
        qm_add_copy_command(${_target}
            SOURCES ${_g2p_packages}/
            DESTINATION $<TARGET_FILE_DIR:${_target}>/../lib/plugins/srt-g2p/G2pPackages
            ${_install_copy_args}
        )
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_linux_rpath_recursive.sh
                --normalize --pattern=lib*.so --except=libonnxruntime*.so
                $<TARGET_FILE_DIR:${_target}>/../lib/plugins
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib
            COMMENT "Fix deployed plugin RPATHs"
        )
    endif()

    # ONNX driver payload declaration (synthrt-owned): the onnxdriver plugin
    # fragment shipped with the synthrt package declares the plugin dir
    # (relative to the deployed plugins/ root), its runtimes subdirectory and
    # the flavors actually deployed. Everything ort-related below derives
    # from that declaration — lite never hardcodes plugin paths or flavors.
    set(_ort_runtimes_rel "")
    if(DEFINED SYNTHRT_ONNXDRIVER_PLUGIN AND DEFINED SYNTHRT_ONNXDRIVER_RUNTIMES_SUBDIR)
        set(_ort_runtimes_rel
            "${SYNTHRT_ONNXDRIVER_PLUGIN}/runtimes/${SYNTHRT_ONNXDRIVER_RUNTIMES_SUBDIR}")
    endif()

    # ONNX Runtime CUDA flavor gate: deployment follows LITE_ENABLE_CUDA,
    # never the vcpkg tree's residue. ON requires the cuda/ payload (fatal if
    # missing), OFF sweeps a stray one with a visible warning — see
    # cmake/OrtRuntimeGate.cmake. macOS never carries a CUDA payload
    # (cuda12 supports x64 & (windows | linux)), so there is nothing to gate.
    if(NOT APPLE AND _ort_runtimes_rel)
        if(WIN32)
            set(_ort_cuda_build_dir
                "$<TARGET_FILE_DIR:${_target}>/plugins/${_ort_runtimes_rel}/cuda")
            set(_ort_cuda_install_dir
                "\${CMAKE_INSTALL_PREFIX}/bin/plugins/${_ort_runtimes_rel}/cuda")
        else()
            set(_ort_cuda_build_dir
                "$<TARGET_FILE_DIR:${_target}>/../lib/plugins/${_ort_runtimes_rel}/cuda")
            set(_ort_cuda_install_dir
                "\${CMAKE_INSTALL_PREFIX}/lib/plugins/${_ort_runtimes_rel}/cuda")
        endif()
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                -D "expect_cuda=$<BOOL:${LITE_ENABLE_CUDA}>"
                -D "cuda_dir=${_ort_cuda_build_dir}"
                -D "phase=build"
                -P "${LITE_CMAKE_DIR}/OrtRuntimeGate.cmake"
            COMMENT "Gate ONNX Runtime CUDA runtimes (LITE_ENABLE_CUDA=${LITE_ENABLE_CUDA})"
            VERBATIM
        )
        install(CODE "
            execute_process(
                COMMAND \"${CMAKE_COMMAND}\"
                    -D expect_cuda=${LITE_ENABLE_CUDA}
                    -D \"cuda_dir=${_ort_cuda_install_dir}\"
                    -D phase=install
                    -P \"${LITE_CMAKE_DIR}/OrtRuntimeGate.cmake\"
                COMMAND_ERROR_IS_FATAL ANY
            )
        ")
    elseif(NOT APPLE)
        message(WARNING
            "The synthrt package has no onnxdriver payload manifest (plugin not "
            "built or synthrt too old); the ONNX Runtime CUDA runtime gate is "
            "skipped and CUDA deployment follows the vcpkg tree as-is. Update "
            "the synthrt package.")
    endif()

    # Debug/RelWithDebInfo build trees stage the ort payload symbols next to
    # the deployed runtime DLLs as well, so debugging the app from the build
    # tree can frame-walk into ONNX Runtime. Mirrors the install-time staging
    # below, but only for flavors actually kept in the build tree (cuda only
    # when LITE_ENABLE_CUDA is ON — a stray one was swept by the gate above
    # and never re-created here), scheduled per file with copy_if_different
    # so incremental builds stay free. Release build trees stay symbol-free.
    if(NOT APPLE AND CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$" AND
       _ort_runtimes_rel AND DEFINED SYNTHRT_ONNXDRIVER_FLAVORS)
        find_package(onnxruntime-builds CONFIG QUIET)
        set(_ort_build_sym_commands "")
        foreach(_flavor IN LISTS SYNTHRT_ONNXDRIVER_FLAVORS)
            if(_flavor STREQUAL "cuda" AND NOT LITE_ENABLE_CUDA)
                continue()
            endif()
            string(TOUPPER "${_flavor}" _flavor_uc)
            if(NOT DEFINED ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR)
                continue()
            endif()
            set(_ort_symbol_dir "${ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR}")
            if(WIN32)
                set(_ort_build_sym_dst
                    "$<TARGET_FILE_DIR:${_target}>/plugins/${_ort_runtimes_rel}/${_flavor}")
            else()
                set(_ort_build_sym_dst
                    "$<TARGET_FILE_DIR:${_target}>/../lib/plugins/${_ort_runtimes_rel}/${_flavor}")
            endif()
            file(GLOB _ort_build_sym_files "${_ort_symbol_dir}/*.pdb")
            foreach(_ort_sym_file IN LISTS _ort_build_sym_files)
                get_filename_component(_ort_sym_name "${_ort_sym_file}" NAME)
                list(APPEND _ort_build_sym_commands COMMAND "${CMAKE_COMMAND}" -E
                    copy_if_different "${_ort_sym_file}" "${_ort_build_sym_dst}/${_ort_sym_name}")
            endforeach()
        endforeach()
        if(_ort_build_sym_commands)
            add_custom_command(TARGET ${_target} POST_BUILD
                ${_ort_build_sym_commands}
                COMMENT "Stage ONNX Runtime ${CMAKE_BUILD_TYPE} symbols into the build tree"
                VERBATIM
            )
        endif()
    endif()

    # RelWithDebInfo/Debug installs carry the ONNX Runtime payload's debug
    # symbols next to their DLLs (portable PDB-included spec). Symbols are
    # consumed through the declared interfaces only: flavors from the
    # synthrt onnxdriver payload manifest, symbol dirs from the
    # onnxruntime-builds per-flavor SYMBOLS_DIR variables — no hardcoded
    # plugin paths, flavors or port layout. Linux ships no symbols (declared
    # dir may not exist at all → EXISTS guard no-op); macOS dSYM bundles are
    # directories and are installed as such. A flavor whose runtime directory was not
    # staged (cuda/ swept by the gate above) or whose symbol dir is
    # undefined (cuda12 not installed) is skipped, so this never resurrects
    # a swept directory.
    find_package(onnxruntime-builds CONFIG QUIET)
    if(LITE_INSTALL AND
       CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        set(_ort_symbols_block ON)
        if(NOT _ort_runtimes_rel OR NOT DEFINED SYNTHRT_ONNXDRIVER_FLAVORS)
            message(WARNING
                "The synthrt package has no onnxdriver payload manifest (plugin "
                "not built or synthrt too old); ONNX Runtime debug symbols will "
                "not be staged next to the plugin runtimes. Update the synthrt "
                "package.")
            set(_ort_symbols_block OFF)
        elseif(NOT DEFINED ONNXRUNTIME_BUILDS_SYMBOLS_DIR)
            message(WARNING
                "The onnxruntime-builds package was not found; ONNX Runtime "
                "debug symbols will not be staged next to the plugin runtimes.")
            set(_ort_symbols_block OFF)
        endif()
        if(_ort_symbols_block)
            if(WIN32)
                set(_lite_plugins_root "\${CMAKE_INSTALL_PREFIX}/bin/plugins")
            else()
                set(_lite_plugins_root "\${CMAKE_INSTALL_PREFIX}/lib/plugins")
            endif()
            # Embed flavor + symbol dir literally per install(CODE) block:
            # install scripts run in a fresh scope and cannot see
            # configure-time variables — an "IN LISTS <var>" indirection
            # would silently iterate zero times at install time.
            foreach(_flavor IN LISTS SYNTHRT_ONNXDRIVER_FLAVORS)
                string(TOUPPER "${_flavor}" _flavor_uc)
                if(NOT DEFINED ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR)
                    continue()
                endif()
                set(_ort_symbol_dir "${ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR}")
                if(NOT _ort_symbol_dir)
                    continue()
                endif()
                install(CODE "
                    set(_dst \"${_lite_plugins_root}/${_ort_runtimes_rel}/${_flavor}\")
                    set(_sym_dir \"${_ort_symbol_dir}\")
                    if(EXISTS \"\${_dst}\" AND EXISTS \"\${_sym_dir}\")
                        file(GLOB _ort_symbol_entries \"\${_sym_dir}/*\")
                        foreach(_ort_entry IN LISTS _ort_symbol_entries)
                            get_filename_component(_ort_entry_name \"\${_ort_entry}\" NAME)
                            if(NOT EXISTS \"\${_dst}/\${_ort_entry_name}\")
                                if(IS_DIRECTORY \"\${_ort_entry}\")
                                    file(INSTALL DESTINATION \"\${_dst}\" TYPE DIRECTORY
                                        FILES \"\${_ort_entry}\")
                                else()
                                    file(INSTALL DESTINATION \"\${_dst}\" TYPE FILE
                                        FILES \"\${_ort_entry}\")
                                endif()
                            endif()
                        endforeach()
                    endif()
                ")
            endforeach()
        endif()
    endif()

    # Plugin binaries come from the vcpkg tree, where the synthrt port keeps
    # PDBs next to the plugin DLLs (vcpkg_copy_pdbs covers lib/plugins), so
    # the directory copy above stages them for Debug/RelWithDebInfo installs.
    # Release-family installs sweep them (and any macOS dSYM bundles) to
    # honor the symbol-free installer spec — the same config-driven spirit
    # as the ort gate above.
    if(LITE_INSTALL AND
       NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        if(WIN32)
            set(_lite_plugins_root "\${CMAKE_INSTALL_PREFIX}/bin/plugins")
        else()
            set(_lite_plugins_root "\${CMAKE_INSTALL_PREFIX}/lib/plugins")
        endif()
        install(CODE "
            file(GLOB_RECURSE _lite_plugin_pdbs
                \"${_lite_plugins_root}/*.pdb\")
            foreach(_lite_plugin_pdb IN LISTS _lite_plugin_pdbs)
                file(REMOVE \"\${_lite_plugin_pdb}\")
            endforeach()
            file(GLOB_RECURSE _lite_plugin_dsyms
                \"${_lite_plugins_root}/*.dSYM\")
            foreach(_lite_plugin_dsym IN LISTS _lite_plugin_dsyms)
                file(REMOVE_RECURSE \"\${_lite_plugin_dsym}\")
            endforeach()
        ")
    endif()

    if(LITE_INSTALL AND _deploy_tool AND WIN32)
        install(CODE "
            file(GLOB _lite_runtime_dlls \"$<TARGET_FILE_DIR:${_target}>/*.dll\")
            if(NOT _lite_runtime_dlls)
                message(FATAL_ERROR \"No runtime DLLs found next to $<TARGET_FILE_NAME:${_target}>\")
            endif()
            file(INSTALL
                DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}\"
                TYPE FILE
                FILES \${_lite_runtime_dlls}
            )
        ")

        install(CODE "
            execute_process(
                COMMAND \"${_deploy_tool}\"
                    --libdir \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}\"
                    --plugindir \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}/plugins\"
                    --translations zh_CN
                    --no-system-d3d-compiler
                    --no-compiler-runtime
                    --no-opengl-sw
                    # --pdb only reaches symbol-carrying configs: this genex is
                    # evaluated before the install step runs, so Release-family
                    # installs never stage Qt PDBs (installer spec: no PDBs).
                    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--pdb>
                    --force
                    --verbose 0
                    \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}/$<TARGET_FILE_NAME:${_target}>\"
                WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}\"
            )
        ")
    endif()
endfunction()
