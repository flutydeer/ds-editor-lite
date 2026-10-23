include_guard(DIRECTORY)

# Read deployment paths from SynthrtEngine/DeployLayout.h.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../src/libs/SynthrtEngine/DeployLayout.h" _lite_layout_lines
     REGEX "inline constexpr char [A-Z_]+\\[\\] = \"[^\"]*\";")
foreach(_lite_layout_line IN LISTS _lite_layout_lines)
    string(REGEX MATCH "char ([A-Z_]+)\\[\\] = \"([^\"]*)\"" _ "${_lite_layout_line}")
    set(LITE_LAYOUT_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()
set(LITE_LAYOUT_PLUGIN_CATEGORY_NAMES
    DSINFER_INFERENCE_DIR WOLF_INFERENCE_DIR OTTER_INFERENCE_DIR DSINFER_SINGER_DIR
    WOLF_LINGUIST_DIR INFERENCE_DRIVER_DIR)
foreach(_lite_layout_name IN LISTS LITE_LAYOUT_PLUGIN_CATEGORY_NAMES
                         ITEMS ONNX_RUNTIME_DIR CUDA_RUNTIME_SUBDIR LANGUAGE_PACKAGES_DIR
                               PLUGIN_LIBRARIES)
    if(NOT LITE_LAYOUT_${_lite_layout_name})
        message(FATAL_ERROR "DeployLayout.h does not define ${_lite_layout_name} in the form that "
                            "LiteBuildApi.cmake parses")
    endif()
endforeach()
unset(_lite_layout_lines)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_LIST_DIR}/../src/libs/SynthrtEngine/DeployLayout.h")

function(lite_add_test _target)
    lite_add_executable(${_target}
        TEST
        QT_AUTOGEN
        NO_INSTALL
        ${ARGN}
    )
    target_link_libraries(${_target} PRIVATE Qt6::Test)
    target_include_directories(${_target} PRIVATE "${LITE_SOURCE_DIR}/src/tests")
    add_dependencies(lite_tests ${_target})
endfunction()

function(lite_register_test _target)
    cmake_parse_arguments(TEST "GUI;NATIVE_GUI;RESOURCE_TEST" "NAME;CATEGORY;TIMEOUT;RESOURCE_LOCK" "ARGS" ${ARGN})
    if(NOT TEST_CATEGORY)
        message(FATAL_ERROR "${_target}: a test category is required")
    endif()
    if(NOT TEST_TIMEOUT)
        set(TEST_TIMEOUT 60)
    endif()
    if(NOT TEST_NAME)
        set(TEST_NAME ${_target})
    endif()
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${_target}> -v1 ${TEST_ARGS})
    set(_labels ${TEST_CATEGORY})
    set(_locks ${TEST_RESOURCE_LOCK})
    if(TEST_NATIVE_GUI)
        list(APPEND _labels native)
        list(APPEND _locks desktop)
    elseif(TEST_RESOURCE_TEST)
        list(APPEND _labels resources)
    else()
        list(APPEND _labels ci)
    endif()
    # Keep Qt Test's watchdog from aborting long resource workflows before their own deadline.
    math(EXPR _function_timeout_ms "${TEST_TIMEOUT} * 1000")
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "${_labels}"
        TIMEOUT ${TEST_TIMEOUT}
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${_target}>"
        ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QTEST_FUNCTION_TIMEOUT=${_function_timeout_ms}"
    )
    if(_locks)
        set_tests_properties(${TEST_NAME} PROPERTIES RESOURCE_LOCK "${_locks}")
    endif()
    if(TEST_GUI AND NOT TEST_NATIVE_GUI)
        set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    if(TEST_GUI OR TEST_NATIVE_GUI)
        set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT
            "QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>")
    endif()
    if(WIN32)
        set_property(TEST ${TEST_NAME} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
            "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin")
    endif()
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

    # Windows stages Qt separately from the inference plugins below.
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

    # Preserve the plugins/<library>/<category> layout expected by the runtime.
    set(_lite_vcpkg_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    # Multi-config generators select debug or release paths at build time.
    get_property(_lite_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_lite_debug_only OFF)
    if(NOT _lite_multi_config AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_lite_debug_only ON)
    endif()
    if(_lite_debug_only)
        set(_lite_plugin_source "${_lite_vcpkg_root}/debug/lib")
    elseif(_lite_multi_config)
        set(_lite_plugin_source
            "$<IF:$<CONFIG:Debug>,${_lite_vcpkg_root}/debug/lib,${_lite_vcpkg_root}/lib>")
    else()
        set(_lite_plugin_source "${_lite_vcpkg_root}/lib")
    endif()
    # Stage the runtime paths declared by onnxruntime-builds beside the ONNX driver.
    # The CUDA payload is additional to the default payload.
    find_package(onnxruntime-builds CONFIG QUIET)
    set(_lite_ort_source "")
    if(DEFINED ONNXRUNTIME_BUILDS_RUNTIME_DIR)
        set(_lite_ort_source "${ONNXRUNTIME_BUILDS_RUNTIME_DIR}")
    endif()
    set(_lite_ort_relative "${LITE_LAYOUT_ONNX_RUNTIME_DIR}")

    if(WIN32)
        set(_lite_plugin_destination ".")
        # cpp-pinyin2 is a runtime dependency of wolf's dynamically loaded pinyin plugin.
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE:cpp-pinyin2::cpp-pinyin2>
            DESTINATION .
            ${_install_copy_args}
        )
    elseif(APPLE)
        set(_lite_plugin_destination $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns)
    else()
        set(_lite_plugin_destination $<TARGET_FILE_DIR:${_target}>/../lib)
    endif()

    set(_lite_lang_packages_rel "${LITE_LAYOUT_LANGUAGE_PACKAGES_DIR}")
    set(_lite_ort_cuda_rel "${_lite_ort_relative}/${LITE_LAYOUT_CUDA_RUNTIME_SUBDIR}")

    # Use qmsetup's install root, including its optional debug prefix.
    # Escape it for evaluation in the generated install(CODE) blocks.
    if(WIN32)
        set(_lite_install_root "\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}")
    else()
        set(_lite_install_root "\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_LIBRARY_DIR}")
    endif()
    set(_lite_plugins_root "${_lite_install_root}/plugins")

    # Prefer the plugin directories exported by each package, with vcpkg paths as fallback.
    string(REPLACE " " ";" _lite_plugin_libraries "${LITE_LAYOUT_PLUGIN_LIBRARIES}")
    foreach(_library IN LISTS _lite_plugin_libraries)
        string(TOUPPER "${_library}" _lite_plugin_key)
        set(_lite_plugin_release "${_lite_vcpkg_root}/lib/plugins/${_library}")
        set(_lite_plugin_debug "")
        if(IS_DIRECTORY "${_lite_vcpkg_root}/debug/lib/plugins/${_library}")
            set(_lite_plugin_debug "${_lite_vcpkg_root}/debug/lib/plugins/${_library}")
        endif()
        if(DEFINED ${_lite_plugin_key}_PLUGINS_DIR)
            set(_lite_plugin_release "${${_lite_plugin_key}_PLUGINS_DIR}")
            # Older package configs may omit _PLUGINS_DIR_DEBUG even when a debug tree exists.
            if(DEFINED ${_lite_plugin_key}_PLUGINS_DIR_DEBUG)
                set(_lite_plugin_debug "${${_lite_plugin_key}_PLUGINS_DIR_DEBUG}")
            endif()
        endif()

        set(_lite_plugin_tree "${_lite_plugin_release}")
        if(_lite_plugin_debug)
            if(_lite_debug_only)
                set(_lite_plugin_tree "${_lite_plugin_debug}")
            elseif(_lite_multi_config)
                set(_lite_plugin_tree
                    "$<IF:$<CONFIG:Debug>,${_lite_plugin_debug},${_lite_plugin_release}>")
            endif()
        endif()

        # Check category directories against the engine's shared deployment layout.
        if(EXISTS "${_lite_plugin_release}")
            foreach(_lite_layout_name IN LISTS LITE_LAYOUT_PLUGIN_CATEGORY_NAMES)
                set(_lite_layout_path "${LITE_LAYOUT_${_lite_layout_name}}")
                if(_lite_layout_path MATCHES "^plugins/${_library}/(.+)$"
                   AND NOT IS_DIRECTORY "${_lite_plugin_release}/${CMAKE_MATCH_1}")
                    message(WARNING
                        "The ${_library} plugin tree at ${_lite_plugin_release} has no "
                        "${CMAKE_MATCH_1} directory, which DeployLayout.h records as "
                        "${_lite_layout_name}; the engine will find no plugins there.")
                endif()
            endforeach()
        endif()

        # Check concrete paths, not the configuration-dependent generator expression.
        if(EXISTS "${_lite_plugin_release}" OR EXISTS "${_lite_plugin_debug}")
            set(_lite_plugin_stage_script
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/LiteStagePluginTree.cmake")
            set(_lite_plugin_stage_destination
                "${_lite_plugin_destination}/plugins/${_library}")
            if(WIN32)
                set(_lite_plugin_stage_destination
                    "$<TARGET_FILE_DIR:${_target}>/plugins/${_library}")
            endif()
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-Dsource=${_lite_plugin_tree}"
                    "-Ddestination=${_lite_plugin_stage_destination}"
                    -P "${_lite_plugin_stage_script}"
                VERBATIM
            )
            if(LITE_INSTALL)
                # Preserve qmsetup's build-relative install layout on every platform.
                install(CODE "
                    file(RELATIVE_PATH _plugin_relative
                        \"${QMSETUP_BUILD_DIR}\" \"${_lite_plugin_stage_destination}\")
                    execute_process(COMMAND \"${CMAKE_COMMAND}\"
                        \"-Dsource=${_lite_plugin_tree}\"
                        \"-Ddestination=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/\${_plugin_relative}\"
                        -P \"${_lite_plugin_stage_script}\"
                        COMMAND_ERROR_IS_FATAL ANY)
                ")
            endif()
        else()
            message(WARNING
                "No ${_library} plugin tree at ${_lite_plugin_release}; the ${_library} "
                "plugins will be unavailable at runtime.")
        endif()
    endforeach()

    # Resolve language resources from an explicit path, the environment, or the data port.
    # The sibling checkout fallback requires LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK.
    # Installable builds require a valid package tree.
    set(LITE_WOLF_LANG_PACKAGES "" CACHE PATH
        "Unpacked wolf language packages to stage; empty resolves them by convention")
    if(NOT LITE_WOLF_LANG_PACKAGES AND DEFINED ENV{WOLF_LANG_PACKAGES_SOURCE})
        set(LITE_WOLF_LANG_PACKAGES "$ENV{WOLF_LANG_PACKAGES_SOURCE}")
    endif()
    if(NOT LITE_WOLF_LANG_PACKAGES)
        find_package(wolf-lang-packages CONFIG QUIET)
        if(wolf-lang-packages_FOUND)
            set(LITE_WOLF_LANG_PACKAGES "${WOLF_LANG_PACKAGES_DIR}")
        endif()
    endif()
    option(LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK
        "Stage the language packages of a sibling wolf checkout if no other source is set" OFF)
    set(_lite_lang_sibling "${CMAKE_SOURCE_DIR}/../wolf/build/lang-packages")
    if(NOT LITE_WOLF_LANG_PACKAGES AND LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK
       AND IS_DIRECTORY "${_lite_lang_sibling}")
        set(LITE_WOLF_LANG_PACKAGES "${_lite_lang_sibling}")
    endif()

    string(CONCAT _lite_lang_hint
        "Set LITE_WOLF_LANG_PACKAGES, or the WOLF_LANG_PACKAGES_SOURCE environment variable, to "
        "the directory of unpacked packages (one subdirectory per package, each containing its "
        "desc.json), or install the wolf-lang-packages port listed in the manifest. "
        "A sibling wolf checkout keeps a development copy at ${_lite_lang_sibling}, which this "
        "build stages only with LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK=ON.")

    set(_lite_lang_manifests "")
    set(_lite_lang_names "")
    set(_lite_lang_problem "")
    if(NOT LITE_WOLF_LANG_PACKAGES)
        string(CONCAT _lite_lang_problem
            "no source is set: LITE_WOLF_LANG_PACKAGES and the WOLF_LANG_PACKAGES_SOURCE "
            "environment variable are empty, no wolf-lang-packages package is installed, and "
            "LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK is off or ${_lite_lang_sibling} is not a "
            "directory")
    elseif(NOT IS_DIRECTORY "${LITE_WOLF_LANG_PACKAGES}")
        set(_lite_lang_problem "${LITE_WOLF_LANG_PACKAGES} is not a directory")
    else()
        # Copy package directories only; the source tree may also contain archives or duplicate
        # bundles.
        file(GLOB _lite_lang_manifests "${LITE_WOLF_LANG_PACKAGES}/*/desc.json")
        if(NOT _lite_lang_manifests)
            set(_lite_lang_problem
                "${LITE_WOLF_LANG_PACKAGES} contains no package (no */desc.json)")
        endif()
    endif()

    if(NOT _lite_lang_problem)
        foreach(_manifest IN LISTS _lite_lang_manifests)
            get_filename_component(_package "${_manifest}" DIRECTORY)
            get_filename_component(_package_name "${_package}" NAME)
            list(APPEND _lite_lang_names "${_package_name}")
            qm_add_copy_command(${_target}
                SOURCES ${_package}/
                DESTINATION ${_lite_plugin_destination}/${_lite_lang_packages_rel}/${_package_name}
                ${_install_copy_args}
            )
        endforeach()
        list(LENGTH _lite_lang_manifests _lite_lang_count)
        message(STATUS "Staging ${_lite_lang_count} wolf language package(s) from "
            "${LITE_WOLF_LANG_PACKAGES}")
    elseif(LITE_INSTALL)
        message(FATAL_ERROR
            "No wolf language packages staged for an installed tree: ${_lite_lang_problem}. An "
            "installer built from this tree would load voicebanks and resolve no wolf/lang-* "
            "dependency. ${_lite_lang_hint}")
    else()
        message(WARNING
            "No wolf language packages staged: ${_lite_lang_problem}. Voicebanks will load and "
            "resolve no wolf/lang-* dependency, so every language that requires such a dependency "
            "is unavailable. "
            "${_lite_lang_hint}")
    endif()

    # qmsetup can succeed without copying any files when a source disappears.
    # Verify the staged tree after all copy commands have run.
    if(APPLE)
        set(_lite_lang_stage_dir
            "$<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/${_lite_lang_packages_rel}")
    elseif(WIN32)
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/${_lite_lang_packages_rel}")
    else()
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/../lib/${_lite_lang_packages_rel}")
    endif()
    # Pass expected package names with commas to preserve them as one command-line argument.
    string(REPLACE ";" "," _lite_lang_expected "${_lite_lang_names}")
    add_custom_command(TARGET ${_target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            -D "LITE_STAGED_LANG_PACKAGES_DIR=${_lite_lang_stage_dir}"
            -D "expected=${_lite_lang_expected}"
            -D "land_point=${_lite_lang_packages_rel}"
            -D "source=${LITE_WOLF_LANG_PACKAGES}"
            -D "problem=${_lite_lang_problem}"
            -D "hint=${_lite_lang_hint}"
            -D "install_enabled=${LITE_INSTALL}"
            # Check the install script for the directory that registers these copies.
            -D "install_script=${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake"
            -D "LITE_INSTALL_DIR_USE_DEBUG_PREFIX=${LITE_INSTALL_DIR_USE_DEBUG_PREFIX}"
            -P "${LITE_CMAKE_DIR}/LitePackagingLayoutCheck.cmake"
        COMMENT "Check the wolf language packages staged into the build tree"
        VERBATIM
    )

    if(EXISTS "${_lite_ort_source}")
        qm_add_copy_command(${_target}
            SOURCES ${_lite_ort_source}/
            DESTINATION ${_lite_plugin_destination}/${_lite_ort_relative}
            ${_install_copy_args}
        )
    else()
        # Installable builds require the default runtime payload; development builds only warn.
        if(LITE_INSTALL)
            set(_lite_ort_gate FATAL_ERROR)
        else()
            set(_lite_ort_gate WARNING)
        endif()
        if(_lite_ort_source STREQUAL "")
            message(${_lite_ort_gate}
                "The onnxruntime-builds package declares no ONNX Runtime payload "
                "(ONNXRUNTIME_BUILDS_RUNTIME_DIR is unset). Packaging must carry the payload into "
                "${_lite_ort_relative}; without it the editor will list voicebanks, but synthesis "
                "will be unavailable.")
        else()
            message(${_lite_ort_gate}
                "No ONNX Runtime payload at ${_lite_ort_source}. Packaging must carry the payload "
                "into ${_lite_ort_relative}; without it the editor will list voicebanks, but "
                "synthesis will be unavailable.")
        endif()
    endif()

    # Keep CUDA in a separate subdirectory because its DLL names overlap the default payload.
    # Register its copy before the runtime gate.
    if(NOT APPLE AND LITE_ENABLE_CUDA AND DEFINED ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR)
        qm_add_copy_command(${_target}
            SOURCES ${ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR}/
            DESTINATION ${_lite_plugin_destination}/${_lite_ort_cuda_rel}
            ${_install_copy_args}
        )
    elseif(NOT APPLE AND LITE_ENABLE_CUDA)
        # Remove stale CUDA payloads when no source is declared, so the runtime gate detects the
        # loss.
        if(WIN32)
            set(_ort_cuda_stale_dir "$<TARGET_FILE_DIR:${_target}>/${_lite_ort_cuda_rel}")
        else()
            set(_ort_cuda_stale_dir
                "$<TARGET_FILE_DIR:${_target}>/../lib/${_lite_ort_cuda_rel}")
        endif()
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_ort_cuda_stale_dir}"
            COMMENT "Sweep a stale ONNX Runtime CUDA payload (LITE_ENABLE_CUDA=ON, none declared)"
            VERBATIM
        )
    endif()

    if(UNIX AND NOT APPLE)
        # Copy transitive plugin dependencies that executable-only deployment does not discover.
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/deploy_linux_plugin_deps.sh
                $<TARGET_FILE_DIR:${_target}>/../lib
                ${_lite_plugin_source}
                ${_lite_vcpkg_root}/lib
            COMMENT "Deploy the shared libraries the plugins need"
        )
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_linux_rpath_recursive.sh
                --normalize --pattern=lib*.so --except=libonnxruntime*.so
                $<TARGET_FILE_DIR:${_target}>/../lib
                $<TARGET_FILE_DIR:${_target}>/../lib
            COMMENT "Fix deployed plugin RPATHs"
        )
    endif()

    set(_ort_runtimes_rel "${_lite_ort_relative}")

    # ONNX Runtime CUDA flavor gate: deployment follows LITE_ENABLE_CUDA,
    # never the vcpkg tree's residue. ON requires the cuda/ payload (fatal if
    # missing), OFF sweeps a stray one with a visible warning — see
    # cmake/OrtRuntimeGate.cmake. macOS never carries a CUDA payload
    # (cuda12 supports x64 & (windows | linux)), so there is nothing to gate.
    if(NOT APPLE AND _ort_runtimes_rel)
        if(WIN32)
            set(_ort_cuda_build_dir
                "$<TARGET_FILE_DIR:${_target}>/${_lite_ort_cuda_rel}")
            set(_ort_cuda_install_dir
                "${_lite_install_root}/${_lite_ort_cuda_rel}")
        else()
            set(_ort_cuda_build_dir
                "$<TARGET_FILE_DIR:${_target}>/../lib/${_lite_ort_cuda_rel}")
            set(_ort_cuda_install_dir
                "${_lite_install_root}/${_lite_ort_cuda_rel}")
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
            "No ONNX Runtime runtime directory is named (_lite_ort_relative is empty); "
            "the ONNX Runtime CUDA gate is skipped and CUDA deployment follows the "
            "vcpkg tree as-is.")
    endif()

    # Stage runtime symbols for Debug and RelWithDebInfo, only for enabled payloads.
    if(NOT APPLE AND (CMAKE_CONFIGURATION_TYPES OR
       CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$") AND
       _ort_runtimes_rel AND DEFINED ONNXRUNTIME_BUILDS_FLAVORS)
        set(_ort_build_sym_commands "")
        # Multi-config generators choose the configuration at build time. cmake -E true is a
        # no-op for the other configurations, including its unused source/destination arguments.
        set(_ort_symbol_copy_command
            "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,copy_if_different,true>")
        foreach(_flavor IN LISTS ONNXRUNTIME_BUILDS_FLAVORS)
            if(_flavor STREQUAL "cuda" AND NOT LITE_ENABLE_CUDA)
                continue()
            endif()
            string(TOUPPER "${_flavor}" _flavor_uc)
            if(NOT DEFINED ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR)
                continue()
            endif()
            set(_ort_symbol_dir "${ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR}")
            if(NOT _ort_symbol_dir)
                continue()
            endif()
            set(_ort_flavor_rel "")
            if(NOT _flavor STREQUAL "default")
                set(_ort_flavor_rel "/${_flavor}")
            endif()
            if(WIN32)
                set(_ort_build_sym_dst
                    "$<TARGET_FILE_DIR:${_target}>/${_ort_runtimes_rel}${_ort_flavor_rel}")
            else()
                set(_ort_build_sym_dst
                    "$<TARGET_FILE_DIR:${_target}>/../lib/${_ort_runtimes_rel}${_ort_flavor_rel}")
            endif()
            file(GLOB _ort_build_sym_files "${_ort_symbol_dir}/*.pdb")
            foreach(_ort_sym_file IN LISTS _ort_build_sym_files)
                get_filename_component(_ort_sym_name "${_ort_sym_file}" NAME)
                list(APPEND _ort_build_sym_commands COMMAND "${CMAKE_COMMAND}" -E
                    "${_ort_symbol_copy_command}"
                    "${_ort_sym_file}" "${_ort_build_sym_dst}/${_ort_sym_name}")
            endforeach()
        endforeach()
        if(_ort_build_sym_commands)
            add_custom_command(TARGET ${_target} POST_BUILD
                ${_ort_build_sym_commands}
                COMMENT "Stage ONNX Runtime symbols for the selected build configuration"
                VERBATIM
            )
        endif()
    endif()

    # Install symbols only beside existing payloads, preserving dSYM directories.
    # Do not recreate a disabled CUDA payload directory.
    if(LITE_INSTALL AND (CMAKE_CONFIGURATION_TYPES OR
       CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$"))
        if(NOT _ort_runtimes_rel)
            message(WARNING
                "No ONNX Runtime runtime directory is named (_lite_ort_relative is empty); "
                "ONNX Runtime debug symbols will not be staged next to the plugin runtimes.")
        elseif(NOT DEFINED ONNXRUNTIME_BUILDS_SYMBOLS_DIR)
            message(WARNING
                "The onnxruntime-builds package was not found, so no flavor declares a "
                "symbol directory; ONNX Runtime debug symbols will not be staged next to "
                "the plugin runtimes.")
        else()
            # Embed flavor + symbol dir literally per install(CODE) block:
            # install scripts run in a fresh scope and cannot see
            # configure-time variables — an "IN LISTS <var>" indirection
            # would silently iterate zero times at install time.
            foreach(_flavor IN LISTS ONNXRUNTIME_BUILDS_FLAVORS)
                string(TOUPPER "${_flavor}" _flavor_uc)
                if(NOT DEFINED ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR)
                    continue()
                endif()
                set(_ort_symbol_dir "${ONNXRUNTIME_BUILDS_${_flavor_uc}_SYMBOLS_DIR}")
                if(NOT _ort_symbol_dir)
                    continue()
                endif()
                set(_ort_flavor_rel "")
                if(NOT _flavor STREQUAL "default")
                    set(_ort_flavor_rel "/${_flavor}")
                endif()
                install(CODE "
                    if(CMAKE_INSTALL_CONFIG_NAME MATCHES \"^(Debug|RelWithDebInfo)$\")
                        set(_dst \"${_lite_install_root}/${_ort_runtimes_rel}${_ort_flavor_rel}\")
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
    if(LITE_INSTALL)
        install(CODE "
            if(NOT CMAKE_INSTALL_CONFIG_NAME MATCHES \"^(Debug|RelWithDebInfo)$\")
                file(GLOB_RECURSE _lite_plugin_pdbs
                    \"${_lite_plugins_root}/*.pdb\")
                foreach(_lite_plugin_pdb IN LISTS _lite_plugin_pdbs)
                    file(REMOVE \"\${_lite_plugin_pdb}\")
                endforeach()
                file(GLOB_RECURSE _lite_plugin_dsyms LIST_DIRECTORIES true
                    \"${_lite_plugins_root}/*.dSYM\")
                foreach(_lite_plugin_dsym IN LISTS _lite_plugin_dsyms)
                    file(REMOVE_RECURSE \"\${_lite_plugin_dsym}\")
                endforeach()
            endif()
        ")
    endif()

    if(APPLE AND _deploy_tool)
        find_package(ffmpeg-builds CONFIG QUIET)
        if(TARGET FFmpeg::avcodec)
            set_property(TARGET ${_target} APPEND PROPERTY BUILD_RPATH
                "@executable_path/../Frameworks/ffmpeg-builds")
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "$<TARGET_FILE_DIR:FFmpeg::avcodec>/ffmpeg-builds"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${_target}>/Frameworks/ffmpeg-builds"
                VERBATIM
            )
        endif()
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash "${LITE_SOURCE_DIR}/scripts/deploy_macos.sh"
                "${_deploy_tool}" "$<TARGET_BUNDLE_DIR:${_target}>"
            COMMENT "Deploy and sign macOS runtime dependencies"
            VERBATIM
        )
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
