include_guard(DIRECTORY)

# Relative paths below the deployed plugin root that the engine uses at run time. They are defined
# once, in src/libs/SynthrtEngine/DeployLayout.h, and parsed here so that the deployed layout and
# the paths the engine searches cannot diverge.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../src/libs/SynthrtEngine/DeployLayout.h" _lite_layout_lines
     REGEX "inline constexpr char [A-Z_]+\\[\\] = \"[^\"]*\";")
foreach(_lite_layout_line IN LISTS _lite_layout_lines)
    string(REGEX MATCH "char ([A-Z_]+)\\[\\] = \"([^\"]*)\"" _ "${_lite_layout_line}")
    set(LITE_LAYOUT_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()
set(LITE_LAYOUT_PLUGIN_CATEGORY_NAMES
    DSINFER_INFERENCE_DIR WOLF_INFERENCE_DIR OTTER_INFERENCE_DIR DSINFER_SINGER_DIR
    WOLF_LINGUIST_DIR INFERENCE_DRIVER_DIR)
# PLUGIN_LIBRARIES is a space-separated list of library level directory names, and the copies below
# walk it one library at a time. A missing value would degrade the list to empty and the copy loop to
# zero iterations, so it is self-checked here like the other constants.
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

    # Plugin tree source and destination.
    #
    # A category is the unit of plugin discovery. Each of the three packages installs its plugins
    # under `plugins/<library>/<category>`, so a single tree contains all of them. The tree is
    # deployed with the same structure because SynthrtEngine::defaultPluginRoot() returns the
    # directory that contains it and Bootstrap appends the remaining path components.
    set(_lite_vcpkg_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    # The plugin tree (release or debug) is selected per library below, from the variables each
    # package exports. The selection requires two facts: whether the configuration is fixed at
    # configure time and, if so, which configuration it is. A multi-config generator fixes neither
    # at configure time and leaves CMAKE_BUILD_TYPE empty. A plain `STREQUAL "Debug"` test would
    # therefore evaluate as Release, and a Debug build would deploy the release plugins.
    get_property(_lite_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_lite_debug_only OFF)
    if(NOT _lite_multi_config AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_lite_debug_only ON)
    endif()
    # Library directory searched first for the shared libraries that the plugins depend on. It is
    # selected by the same rule as the plugin trees: the debug tree for a single-config Debug
    # build, a per-configuration generator expression for a multi-config build, and the release
    # tree otherwise. The release directory is always passed after it as the fallback, which also
    # covers a release-only vcpkg tree.
    if(_lite_debug_only)
        set(_lite_plugin_source "${_lite_vcpkg_root}/debug/lib")
    elseif(_lite_multi_config)
        set(_lite_plugin_source
            "$<IF:$<CONFIG:Debug>,${_lite_vcpkg_root}/debug/lib,${_lite_vcpkg_root}/lib>")
    else()
        set(_lite_plugin_source "${_lite_vcpkg_root}/lib")
    endif()
    # ONNX Runtime is not a plugin and is not installed with a plugin. The host passes the runtime
    # location to the driver instead of letting the driver search for it, so the runtime is staged
    # beside the driver and nowhere else. The loaded copy is a deployment decision, and a library
    # search can load a different copy installed on the machine.
    #
    # The onnxruntime-builds package declares its payloads and the location of the files of each
    # payload. The CUDA payload staged below exists only if the tree was installed with
    # --x-feature=cuda12. The package is looked up once, in the deployment scope, so that the
    # payload staging and the symbol staging below read the same declaration and do not probe
    # again. The default payload is read from that declaration instead of being derived from the
    # tree layout, so a layout change is reported by the check below rather than silently staging a
    # directory that no longer holds the runtime.
    find_package(onnxruntime-builds CONFIG QUIET)
    set(_lite_ort_source "")
    if(DEFINED ONNXRUNTIME_BUILDS_RUNTIME_DIR)
        set(_lite_ort_source "${ONNXRUNTIME_BUILDS_RUNTIME_DIR}")
    endif()
    set(_lite_ort_relative "${LITE_LAYOUT_ONNX_RUNTIME_DIR}")

    if(WIN32)
        set(_lite_plugin_destination ".")
        qm_add_copy_command(${_target}
            SOURCES $<TARGET_FILE:cpp-pinyin::cpp-pinyin>
            DESTINATION .
            ${_install_copy_args}
        )
    elseif(APPLE)
        set(_lite_plugin_destination $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns)
    else()
        set(_lite_plugin_destination $<TARGET_FILE_DIR:${_target}>/../lib)
    endif()

    # Two paths below the plugin destination are referenced outside this build: the destination of
    # the wolf language packages and the destination of the ONNX Runtime CUDA payload. The copy
    # commands below stage both. packaging/windows/build-installer.ps1 and build-portable.ps1 read
    # them from DeployLayout.h, as this file does, and verify them against a finished install. They
    # are defined once here so that all copy commands below use the same values.
    set(_lite_lang_packages_rel "${LITE_LAYOUT_LANGUAGE_PACKAGES_DIR}")
    set(_lite_ort_cuda_rel "${_lite_ort_relative}/${LITE_LAYOUT_CUDA_RUNTIME_SUBDIR}")

    # Root of the deployed tree under the install prefix: the runtime directory on Windows and the
    # library directory elsewhere, matching the destination of the copy commands above. The
    # relative paths below already begin with `plugins/` (for example _lite_ort_relative), so the
    # root does not include it. A doubled prefix would specify a directory into which nothing is
    # staged; the CUDA gate would report an absent payload, and the symbol staging would search an
    # empty directory.
    # The directory name is taken from qmsetup instead of being hardcoded. The qmsetup build API
    # defines the install layout (BuildRepoHelpers.cmake:211/212 derive <prefix>_INSTALL_RUNTIME_DIR
    # and _LIBRARY_DIR from <prefix>_INSTALL_BASE_DIR, and src/CMakeLists.txt:3 sets the prefix of
    # those names to LITE). The same two variables place the application, its PDB and the Qt
    # deployment, so a single source defines the location of the tree. A hardcoded "bin" would
    # diverge from them if LITE_INSTALL_DIR_USE_DEBUG_PREFIX is enabled: qmsetup would install
    # below <prefix>/debug, and this root would specify a directory that nothing writes to.
    # The values are escaped because they are embedded in install(CODE) blocks, which run in a
    # separate scope.
    if(WIN32)
        set(_lite_install_root "\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}")
    else()
        set(_lite_install_root "\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_LIBRARY_DIR}")
    endif()
    set(_lite_plugins_root "${_lite_install_root}/plugins")

    # Each package exports the location of its plugins: dsinfer, wolf and otter all export
    # <LIB>_PLUGINS_DIR, and also <LIB>_PLUGINS_DIR_DEBUG if the tree contains a debug build. The
    # exported values are read rather than recomputed, so that a package that relocates its tree
    # relocates the deployment source as well. The path derived from the vcpkg layout remains the
    # fallback for a package too old to export these variables.
    #
    # The library level directory names come from PLUGIN_LIBRARIES in DeployLayout.h, and both
    # packaging scripts assert the same list, so the deploy and verify sides cannot drift apart. The
    # separator conversion follows the existing practice of this file: string(REPLACE) turns the
    # space-separated text into a CMake list.
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
            # A package that exports its release plugin directory but not its debug plugin
            # directory predates the _DEBUG variable and does not necessarily lack a debug build.
            # The derived debug tree therefore remains correct for such a package, and discarding
            # that tree would deploy release plugins into a Debug host.
            if(DEFINED ${_lite_plugin_key}_PLUGINS_DIR_DEBUG)
                set(_lite_plugin_debug "${${_lite_plugin_key}_PLUGINS_DIR_DEBUG}")
            endif()
        endif()

        # Tree selection follows the rule stated above: a single-config Debug build uses the debug
        # tree directly, a multi-config build receives a generator expression that selects the tree
        # per configuration at build time, and a library without a debug tree uses the release tree.
        set(_lite_plugin_tree "${_lite_plugin_release}")
        if(_lite_plugin_debug)
            if(_lite_debug_only)
                set(_lite_plugin_tree "${_lite_plugin_debug}")
            elseif(_lite_multi_config)
                set(_lite_plugin_tree
                    "$<IF:$<CONFIG:Debug>,${_lite_plugin_debug},${_lite_plugin_release}>")
            endif()
        endif()

        # The engine searches for each category below the deployed tree by the directory names
        # that DeployLayout.h records. None of the libraries exports those names. This check
        # therefore detects a renamed category directory at configure time instead of at run time
        # as a category without plugins.
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

        # The existence check tests the two paths rather than the generator expression, because
        # the build uses one of those two paths.
        if(EXISTS "${_lite_plugin_release}" OR EXISTS "${_lite_plugin_debug}")
            qm_add_copy_command(${_target}
                SOURCES ${_lite_plugin_tree}/
                DESTINATION ${_lite_plugin_destination}/plugins/${_library}
                ${_install_copy_args}
            )
        else()
            message(WARNING
                "No ${_library} plugin tree at ${_lite_plugin_release}; the ${_library} "
                "plugins will be unavailable at runtime.")
        endif()
    endforeach()

    # Language packages that voicebanks depend on. The wolf project publishes them as data: its
    # scripts build them from resources instead of compiling them, and a host receives a Package
    # search root for them. The wolf-lang-packages port exports that root as
    # WOLF_LANG_PACKAGES_DIR, and the manifest includes the port, so a clean checkout stages the
    # published release. An unpacked copy of the same tree can replace it. The sources below are
    # tried in the listed order, and none of them is an untracked machine-local preset.
    #
    #   1. LITE_WOLF_LANG_PACKAGES       the cache variable of this repository
    #   2. WOLF_LANG_PACKAGES_SOURCE     the environment variable read by wolf's CMake and tests
    #   3. wolf-lang-packages package    the root of the port, if the manifest installed it
    #   4. ../wolf/build/lang-packages   the sibling checkout convention: the output directory of
    #                                    the conversion script of that repository and the default
    #                                    of its other scripts. The versioned directories beside it
    #                                    are not used: two bundles can coexist there, and neither
    #                                    tree records which bundle this build requires, so a
    #                                    version is selected by source 1 or 2 and never inferred
    #                                    from a directory listing. Consulted only if
    #                                    LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK is ON: the
    #                                    manifest installs source 3, and a CI or packaging machine
    #                                    with a wolf checkout beside this one must not stage the
    #                                    development output of that checkout instead.
    #
    # A tree that stages no language packages still loads voicebanks but resolves no language.
    # This degradation is acceptable in a developer build and produces a defective installer from
    # an installed tree. Configuration of a tree that is to be installed therefore fails without
    # the language packages.
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

    # Built with string(CONCAT) instead of set() with one argument per line: set() would create a
    # semicolon-separated list, and message() below would print it with the semicolons.
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
        # Each package directory is copied individually instead of the parent directory. The
        # parent directory is a build tree that also contains archives of the same packages and a
        # second unpacked copy of each package. Deploying them would place two packages with the
        # same identity in the directory from which the loader resolves dependencies.
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

    # Post-build verification of the copied language packages. The qmsetup copy script
    # (modules/scripts/copy.cmake) does not fail if a source directory globbed at configure time is
    # removed before the build: it copies an empty set and exits with 0, and the tree then contains
    # no package although every step reports success. The check is registered after the copy
    # commands because POST_BUILD commands on a target run in registration order, which the CUDA
    # gate below also relies on. The check uses the destination of the copy commands, so it
    # verifies the directory that the copy commands write.
    if(APPLE)
        set(_lite_lang_stage_dir
            "$<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/${_lite_lang_packages_rel}")
    elseif(WIN32)
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/${_lite_lang_packages_rel}")
    else()
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/../lib/${_lite_lang_packages_rel}")
    endif()
    # The expected package names, not only their count, are passed so that the check also detects
    # a copy that staged nothing while packages from an earlier build remain in the tree. The
    # names are separated by ',' because a ';'-separated CMake list would reach the script as one
    # argument per package.
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
            # Install script of the directory that registers the copies. A build tree contains one
            # cmake_install.cmake per directory, and the copies above are registered in this
            # directory, so the script of this directory must contain the install-tree destination.
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
        # The default payload is a requirement of the installer: without it a package still lists
        # voicebanks while synthesis is unusable, so a missing payload is an error when LITE_INSTALL
        # is true and keeps its warning in a development build, mirroring the FATAL_ERROR / WARNING
        # nesting of the wolf language package gate above. Both branches are real: the Config.cmake
        # shipped by onnxruntime-builds calls set() on ONNXRUNTIME_BUILDS_RUNTIME_DIR without an
        # EXISTS guard, so a set variable that points at a missing directory is a reachable path, and
        # an unset variable is the path taken with an older package.
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

    # The CUDA flavor is an additional payload, not a replacement. Its DLL names collide with those
    # of the default payload, so it is staged in the CUDA_RUNTIME_SUBDIR subdirectory of the
    # runtime directory. cmake/OrtRuntimeGate.cmake checks at build and install time whether that
    # subdirectory must exist; this block only stages the payload, and is registered before the
    # gate (see the post-build check above), so the gate runs after it.
    if(NOT APPLE AND LITE_ENABLE_CUDA AND DEFINED ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR)
        qm_add_copy_command(${_target}
            SOURCES ${ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR}/
            DESTINATION ${_lite_plugin_destination}/${_lite_ort_cuda_rel}
            ${_install_copy_args}
        )
    elseif(NOT APPLE AND LITE_ENABLE_CUDA)
        # LITE_ENABLE_CUDA is ON but the package declares no CUDA payload: the tree was installed
        # without --x-feature=cuda12, or onnxruntime-builds was not found. A build tree that
        # contained the cuda12 payload earlier, or a port that has since removed it, still contains
        # .../runtime/cuda. The gate below only tests whether that directory exists, so it would
        # accept the stale directory as the required payload, and a CUDA build would load the DLLs
        # of the default payload. The directory is removed here, before the gate (see the post-build
        # check above), so that the gate fails with an error about the missing payload. The install
        # tree is not swept: without a declared payload no copy command for it is registered, and
        # the build reaches the FATAL_ERROR of the gate before anything is installed.
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
        # Deployment of the shared libraries that the plugins depend on, before the plugin RPATHs
        # are rewritten to this directory.
        #
        # vcpkg has no applocal deployment on Linux, and applocal deployment covers only the
        # libraries that the executable links. A plugin is loaded dynamically and links libraries
        # that the executable does not link, for example cpp-pinyin. Without this step the RPATH
        # rewrite below breaks those plugins: it replaces a build RPATH into the vcpkg tree with an
        # RPATH to a directory that does not contain the libraries, and loading the plugin fails
        # with an error about a missing dependency instead of an error about the plugin.
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/deploy_linux_plugin_deps.sh
                $<TARGET_FILE_DIR:${_target}>/../lib
                ${_lite_plugin_source}
                ${_lite_vcpkg_root}/lib
            COMMENT "Deploy the shared libraries the plugins need"
        )
        # The plugins are deployed under the same lib directory as their shared library
        # dependencies, so that directory is both the rewrite target and the target of the
        # rewritten RPATHs.
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_linux_rpath_recursive.sh
                --normalize --pattern=lib*.so --except=libonnxruntime*.so
                $<TARGET_FILE_DIR:${_target}>/../lib
                $<TARGET_FILE_DIR:${_target}>/../lib
            COMMENT "Fix deployed plugin RPATHs"
        )
    endif()

    # ONNX driver payload placement (lite-owned). The synthrt package declares no runtime location
    # for its driver because the driver receives the runtime path from the host. The layout is
    # therefore chosen by lite, is the layout staged above, and is defined once here. An earlier
    # synthrt revision declared this location in an onnxdriver payload fragment; no such fragment
    # is included under cmake/ or src/ (there is no include(... OPTIONAL) of an onnxdriver payload
    # file). The placement of the payload is defined here, its flavors are declared by the
    # onnxruntime-builds package below, and neither depends on the synthrt revision pinned by the
    # synthrt overlay port in scripts/vcpkg-ports/.
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

    # Debug/RelWithDebInfo build trees stage the ort payload symbols next to
    # the deployed runtime DLLs as well, so debugging the app from the build
    # tree can frame-walk into ONNX Runtime. Mirrors the install-time staging
    # below, but only for flavors actually kept in the build tree (cuda only
    # when LITE_ENABLE_CUDA is ON — a stray one was swept by the gate above
    # and never re-created here), scheduled per file with copy_if_different
    # so incremental builds stay free. Release build trees stay symbol-free.
    # The flavors and the symbol directory of each flavor come from the onnxruntime-builds
    # declaration looked up above. The symbols of the "default" flavor are staged in the runtime
    # directory itself because its payload is staged there; the symbols of every other flavor are
    # staged in the subdirectory that contains its payload.
    if(NOT APPLE AND CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$" AND
       _ort_runtimes_rel AND DEFINED ONNXRUNTIME_BUILDS_FLAVORS)
        set(_ort_build_sym_commands "")
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
    # symbols next to their DLLs (portable PDB-included spec). The symbols are
    # consumed through the declared interfaces only, as in the build-tree
    # staging above: no plugin path, flavor or port layout is hardcoded, and
    # the synthrt package is not required to declare anything (see the ONNX
    # driver payload placement above). A flavor whose runtime directory was
    # not staged (cuda/ swept by the gate above, or cuda12 not installed) has
    # no destination directory, and the EXISTS guard below skips it, so this
    # block never re-creates a swept directory. Linux ships no symbols (the
    # declared directory may not exist, and the EXISTS guard then has no
    # effect); macOS dSYM bundles are directories and are installed as such.
    #
    # The remaining warning covers an absent declaration. It names the
    # onnxruntime-builds package, not the synthrt package, which carries no
    # flavor declaration.
    if(LITE_INSTALL AND
       CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
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
                # Same flavor layout as the build-tree symbol staging above.
                set(_ort_flavor_rel "")
                if(NOT _flavor STREQUAL "default")
                    set(_ort_flavor_rel "/${_flavor}")
                endif()
                install(CODE "
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
