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
foreach(_lite_layout_name IN ITEMS ONNX_RUNTIME_DIR CUDA_RUNTIME_SUBDIR LANGUAGE_PACKAGES_DIR)
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

    # Where the plugin tree comes from, and where it goes.
    #
    # On the main line a category is the unit of discovery and each of the three packages installs
    # its own plugins under `plugins/<library>/<category>`, so one tree holds all of them. It is
    # deployed keeping that shape, because SynthrtEngine::defaultPluginRoot() names the directory
    # that holds it and Bootstrap appends the rest.
    set(_lite_vcpkg_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_lite_plugin_source "${_lite_vcpkg_root}/debug/lib")
    else()
        set(_lite_plugin_source "${_lite_vcpkg_root}/lib")
    endif()
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
    # ONNX Runtime is not a plugin and is not installed with one. The driver is told where it is
    # rather than searching, so it is staged beside the driver and nowhere else -- which copy gets
    # loaded is a deployment decision, and a search is how a machine ends up running another one.
    set(_lite_ort_source "${_lite_vcpkg_root}/share/onnxruntime-builds/runtime/default")
    # Which payloads the onnxruntime-builds package carries, and where each one's files are, is
    # declared by that package: the CUDA payload staged below exists only when the tree was
    # installed with --x-feature=cuda12. Looked up once, here in the deployment scope, so that
    # every reader below -- the payload staging and the symbol staging -- reads the same answer
    # instead of probing again.
    find_package(onnxruntime-builds CONFIG QUIET)
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

    # The two paths below the plugin destination that something outside this build names: where
    # the wolf language packages land, and where the ONNX Runtime CUDA payload lands. Both are
    # staged by the copy commands further down, and both are also written out literally by
    # packaging/windows/build-installer.ps1 and build-portable.ps1, which assert them against a
    # finished install instead of reading this build back. Named once here, so the copy commands
    # and the check that reads those scripts cannot answer differently.
    set(_lite_lang_packages_rel "${LITE_LAYOUT_LANGUAGE_PACKAGES_DIR}")
    set(_lite_ort_cuda_rel "${_lite_ort_relative}/${LITE_LAYOUT_CUDA_RUNTIME_SUBDIR}")

    # The deployed tree's root under the install prefix: the target's runtime directory, which is
    # where the copy commands above land. `plugins/` lives below it and is already part of the
    # relative paths below (_lite_ort_relative starts with it), so it is derived once here instead
    # of being prefixed a second time -- a doubled prefix names a directory nothing is ever staged
    # into, which the CUDA gate would read as an absent payload and the symbol staging as a
    # directory to search in vain.
    # The directory name is read back from qmsetup instead of being written down here: its build
    # API owns the install layout (BuildRepoHelpers.cmake:211/212 derive <prefix>_INSTALL_RUNTIME_DIR
    # and _LIBRARY_DIR from <prefix>_INSTALL_BASE_DIR, and src/CMakeLists.txt:3 is what fixes the
    # prefix of those names to LITE), and the same two variables already place the app, its PDB and
    # the Qt deployment, so one authority answers where the tree goes. A hardcoded "bin" here would
    # part company with them the moment LITE_INSTALL_DIR_USE_DEBUG_PREFIX is turned on: qmsetup
    # would install below <prefix>/debug and this root would name a directory nothing writes to.
    # Escaped, because these values are embedded in install(CODE) blocks that run in a scope of
    # their own.
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
    foreach(_library IN ITEMS dsinfer wolf otter)
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

        # A single-config Debug build uses the debug tree directly. A multi-config build cannot
        # select a tree at configure time; it passes the copy command a generator expression, which
        # selects the tree per configuration at build time. In both cases a library without a debug
        # tree uses the release tree, which is the layout of a plain install and of a release-only
        # vcpkg tree.
        set(_lite_plugin_tree "${_lite_plugin_release}")
        if(_lite_plugin_debug)
            if(_lite_debug_only)
                set(_lite_plugin_tree "${_lite_plugin_debug}")
            elseif(_lite_multi_config)
                set(_lite_plugin_tree
                    "$<IF:$<CONFIG:Debug>,${_lite_plugin_debug},${_lite_plugin_release}>")
            endif()
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

    # The language packages a voicebank depends on. The wolf project publishes them as data -- its
    # scripts build them from resources rather than compiling them -- and hands a host a Package
    # search root for them: the wolf-lang-packages port exports it as WOLF_LANG_PACKAGES_DIR, and
    # the manifest includes that port, so a clean checkout stages the published release. An
    # unpacked copy of the same tree can replace it. The four names below are those two spellings
    # and this repository's own, in the order they are tried. The last name is the sibling-checkout
    # convention. None of them is an untracked machine-local preset. Such a preset previously
    # supplied this path, and a clean checkout without that preset staged no language packages.
    #
    #   1. LITE_WOLF_LANG_PACKAGES       this repository's own name for the answer
    #   2. WOLF_LANG_PACKAGES_SOURCE     the name wolf's own CMake and tests read
    #   3. wolf-lang-packages package    the port's root, when the manifest brought it in
    #   4. ../wolf/build/lang-packages   the sibling checkout convention: where that repository's
    #                                    conversion script writes its output and what its own
    #                                    scripts default to. Deliberately not one of the versioned
    #                                    directories beside it -- two bundles can sit there at once
    #                                    and nothing in either tree says which one this build wants,
    #                                    so a version is named by layer 1 or 2, never guessed from a
    #                                    directory listing.
    #
    # A tree that stages none of them still loads voicebanks and resolves no language. That is
    # honest degradation in a developer build and a crippled installer in an installed one, so the
    # tree that is going to be installed refuses to configure without them.
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
    set(_lite_lang_sibling "${CMAKE_SOURCE_DIR}/../wolf/build/lang-packages")
    if(NOT LITE_WOLF_LANG_PACKAGES AND IS_DIRECTORY "${_lite_lang_sibling}")
        set(LITE_WOLF_LANG_PACKAGES "${_lite_lang_sibling}")
    endif()

    # Written through string(CONCAT), not set() with one argument per line: set() would make those
    # a semicolon-separated list and print it that way, while message() below joins its arguments
    # without a separator.
    string(CONCAT _lite_lang_hint
        "Set LITE_WOLF_LANG_PACKAGES, or the WOLF_LANG_PACKAGES_SOURCE environment variable, to "
        "the unpacked copy of the packages themselves -- one subdirectory per package, each "
        "holding its desc.json. A sibling wolf checkout keeps that copy at ${_lite_lang_sibling}, "
        "which is the path this build falls back to; the wolf-lang-packages port is the other "
        "source, once the manifest brings it in.")

    set(_lite_lang_manifests "")
    set(_lite_lang_names "")
    set(_lite_lang_problem "")
    if(NOT LITE_WOLF_LANG_PACKAGES)
        string(CONCAT _lite_lang_problem
            "nothing names them: not LITE_WOLF_LANG_PACKAGES, not the WOLF_LANG_PACKAGES_SOURCE "
            "environment variable, not an installed wolf-lang-packages package, and not a package "
            "tree at ${_lite_lang_sibling}")
    elseif(NOT IS_DIRECTORY "${LITE_WOLF_LANG_PACKAGES}")
        set(_lite_lang_problem "${LITE_WOLF_LANG_PACKAGES} is not a directory")
    else()
        # Each package directory by name, not the directory that holds them. That directory is a
        # build tree: beside the packages sit archives of the same packages and a second unpacked
        # copy of every one of them, and deploying those would put two packages claiming the same
        # identity where the loader resolves dependencies.
        file(GLOB _lite_lang_manifests "${LITE_WOLF_LANG_PACKAGES}/*/desc.json")
        if(NOT _lite_lang_manifests)
            set(_lite_lang_problem
                "${LITE_WOLF_LANG_PACKAGES} holds no package: no */desc.json below it")
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
            "resolve no wolf/lang-* dependency, so every language that needs one is unavailable. "
            "${_lite_lang_hint}")
    endif()

    # The copies above are this build's answer to where the packages go, and nothing checked that
    # they arrived. qmsetup's copy script (modules/scripts/copy.cmake) does not fail on a source
    # directory that went away between the configure that globbed it and the build that copies it:
    # it copies an empty set and exits 0, so a tree can hold no package at all while every step
    # reports success. Registered after those copies -- the order POST_BUILD commands on one target
    # run in, which the CUDA gate below relies on too -- and given the same land point they were
    # given, so the check reads the tree the copy commands write rather than a second opinion of it.
    if(APPLE)
        set(_lite_lang_stage_dir
            "$<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/${_lite_lang_packages_rel}")
    elseif(WIN32)
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/${_lite_lang_packages_rel}")
    else()
        set(_lite_lang_stage_dir "$<TARGET_FILE_DIR:${_target}>/../lib/${_lite_lang_packages_rel}")
    endif()
    # The names, not only how many: with them the check also catches a copy that staged nothing
    # while an earlier build's packages still sit in the tree. Separated by ',' -- a ';' would be a
    # CMake list and would reach the script as one argument per package.
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
            # The install rules of the directory that registers them: a build tree writes one
            # cmake_install.cmake per directory, and the copies above are registered here, so the
            # script beside this directory is the one that has to name the install-tree land point.
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
        message(WARNING
            "No ONNX Runtime payload at ${_lite_ort_source}; the editor will list voicebanks and "
            "refuse to synthesise.")
    endif()

    # The CUDA flavor is a second payload, not a replacement: it carries DLLs whose names collide
    # with the default one's, so it is staged in a subdirectory of the runtime directory. Which
    # subdirectory, and whether it must exist, is what cmake/OrtRuntimeGate.cmake decides at build
    # and install time -- the gate owns the "CUDA=ON but no payload" failure, this only places it.
    # Registered before the gate below, which is a POST_BUILD command on the same target and runs
    # after it in the order they were added.
    if(NOT APPLE AND LITE_ENABLE_CUDA AND DEFINED ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR)
        qm_add_copy_command(${_target}
            SOURCES ${ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR}/
            DESTINATION ${_lite_plugin_destination}/${_lite_ort_cuda_rel}
            ${_install_copy_args}
        )
    elseif(NOT APPLE AND LITE_ENABLE_CUDA)
        # CUDA is on but the package declares no CUDA payload: the tree was installed without
        # --x-feature=cuda12, or onnxruntime-builds was not found at all. A tree that carried
        # cuda12 earlier -- or a port that dropped it since -- still holds .../runtime/cuda, and
        # the gate below only asks whether that directory exists, so it would read the residue as
        # "present as required" and let a CUDA build load the default payload's DLLs. Swept here,
        # before the gate (POST_BUILD commands on one target run in the order they were added), so
        # that the gate instead fails loudly about the payload it cannot find. Nothing sweeps the
        # install tree: with no payload declared no copy of one is registered either, and the build
        # above reaches the gate's FATAL_ERROR long before anything is installed.
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
        # The libraries the plugins need, before their RPATHs are rewritten to look here.
        #
        # vcpkg has no applocal deployment on Linux, and what it would deploy is what the
        # executable links; a plugin is loaded by name and links things the executable never does
        # -- cpp-pinyin, for one. Without this the rewrite below actively breaks them: it replaces
        # a build RPATH pointing into the vcpkg tree with one pointing at a directory those
        # libraries are not in, and the plugin then fails to load complaining about a library
        # rather than about itself.
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/deploy_linux_plugin_deps.sh
                $<TARGET_FILE_DIR:${_target}>/../lib
                ${_lite_plugin_source}
                ${_lite_vcpkg_root}/lib
            COMMENT "Deploy the shared libraries the plugins need"
        )
        # The plugins are deployed under the same lib directory as the shared libraries they
        # need, so that is both what gets rewritten and what the rewritten RPATHs point at.
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_linux_rpath_recursive.sh
                --normalize --pattern=lib*.so --except=libonnxruntime*.so
                $<TARGET_FILE_DIR:${_target}>/../lib
                $<TARGET_FILE_DIR:${_target}>/../lib
            COMMENT "Fix deployed plugin RPATHs"
        )
    endif()

    # ONNX driver payload placement (lite-owned). The refactor line had the synthrt package
    # declare where its driver put its runtimes, so that lite never named a plugin path. The main
    # line does not ship that declaration and does not need to: the driver takes the runtime path
    # from the host, so the layout is lite's own choice and is the one staged above. Named once,
    # here.
    # That declaration is never read, and nothing here would read it if it came back: no fragment
    # of the sort is included anywhere under cmake/ or src/ (there is no include(... OPTIONAL) of
    # an onnxdriver payload file), so the payload's placement and its flavors are lite's own --
    # answered by the onnxruntime-builds package below -- and never depend on which synthrt
    # revision the shared vcpkg overlay happens to pin.
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
    # Both halves of the answer come from the onnxruntime-builds declaration looked up above: the
    # flavors it carries, and each one's own symbol directory. A flavor whose payload was staged in
    # the runtime directory itself ("default") is staged there too; every other flavor is staged in
    # the subdirectory its payload was staged in.
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
    # symbols next to their DLLs (portable PDB-included spec). Symbols are
    # consumed through the declared interfaces only: the flavors and each
    # flavor's symbol directory both come from the onnxruntime-builds
    # package's own declaration -- the same one the payload staging above
    # reads -- so nothing here hardcodes a plugin path, a flavor or the
    # port's layout, and nothing depends on the synthrt package declaring
    # anything (it does not on this line; see above). A flavor whose runtime
    # directory was not staged (cuda/ swept by the gate above, cuda12 not
    # installed) leaves the destination absent and the EXISTS guard below
    # skips it, so this never resurrects a swept directory. Linux ships no
    # symbols (declared dir may not exist at all → EXISTS guard no-op);
    # macOS dSYM bundles are directories and are installed as such.
    #
    # What is left to warn about is data that really is absent: no declaration
    # to read at all. That warning names the package it looked for, not the
    # synthrt package, which carries no flavor declaration on this line.
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
                # The default payload is staged in the runtime directory itself,
                # every other flavor in a subdirectory named after it.
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
