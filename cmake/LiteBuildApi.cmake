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
    cannot be found. qm_win_applocal_deps only copies DLLs reachable from the import
    table, so the Qt plugins (platforms, imageformats, styles) and translations still
    have to come from the official deploy tool.
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
    qm_import(Filesystem Deploy)

    _lite_find_qt_deploy_tool(_deploy_tool)

    # Deploy the Qt runtime first: on macOS the plugins copied below must land inside a
    # bundle macdeployqt has already processed.
    if(_deploy_tool AND WIN32)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${_deploy_tool}"
                --verbose 0
                --translations zh_CN
                --no-system-d3d-compiler
                --no-compiler-runtime
                --no-opengl-sw
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
        )
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/FillLyric/configs/
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/MacOS/configs
            SKIP_INSTALL
        )
    else()
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
            DESTINATION Resources
            INSTALL_DIR .
        )
        qm_add_copy_command(${_target}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/FillLyric/configs/
            DESTINATION configs
            SKIP_INSTALL
        )
    endif()

    set(_g2p_packages
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/synthrt/G2pPackages)

    if(WIN32)
        qm_win_applocal_deps(${_target})
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cpp-pinyin::cpp-pinyin>
                $<TARGET_FILE_DIR:${_target}>
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins
                $<TARGET_FILE_DIR:${_target}>/plugins
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${_g2p_packages}
                $<TARGET_FILE_DIR:${_target}>/plugins/srt-g2p/G2pPackages
            COMMENT "Deploy application runtime dependencies"
        )
    elseif(APPLE)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins
                $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${_g2p_packages}
                $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/srt-g2p/G2pPackages
            COMMENT "Deploy application runtime dependencies"
        )
    elseif(UNIX)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins
                $<TARGET_FILE_DIR:${_target}>/../lib/plugins
            COMMAND bash ${LITE_SOURCE_DIR}/scripts/fix_linux_rpath_recursive.sh
                --normalize --pattern=lib*.so --except=libonnxruntime*.so
                $<TARGET_FILE_DIR:${_target}>/../lib/plugins
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${_g2p_packages}
                $<TARGET_FILE_DIR:${_target}>/../lib/plugins/srt-g2p/G2pPackages
            COMMENT "Deploy application runtime dependencies"
        )
    endif()

    if(LITE_INSTALL)
        _lite_install_application(${_target} "${_deploy_tool}" "${_g2p_packages}")
    endif()
endfunction()

#[[
    Install the runtime dependencies that POST_BUILD copies into the build tree.

    The build-tree copies above are not visible to install(), so the same payload
    (dsinfer plugins, G2P packages, the Qt runtime) has to be declared again here.
]] #
function(_lite_install_application _target _deploy_tool _g2p_packages)
    set(_dsinfer_plugins $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins)

    if(APPLE)
        install(DIRECTORY ${_dsinfer_plugins}
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns
        )
        install(DIRECTORY ${_g2p_packages}
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns/srt-g2p
        )
        return()
    endif()

    if(WIN32)
        install(FILES $<TARGET_FILE:cpp-pinyin::cpp-pinyin> DESTINATION .)
        install(DIRECTORY ${_dsinfer_plugins} DESTINATION .)
        install(DIRECTORY ${_g2p_packages} DESTINATION plugins/srt-g2p)
    else()
        install(DIRECTORY ${_dsinfer_plugins} DESTINATION lib)
        install(DIRECTORY ${_g2p_packages} DESTINATION lib/plugins/srt-g2p)
    endif()

    if(_deploy_tool AND WIN32)
        install(CODE "
            execute_process(
                COMMAND \"${_deploy_tool}\"
                    --libdir \"\${CMAKE_INSTALL_PREFIX}\"
                    --plugindir \"\${CMAKE_INSTALL_PREFIX}/plugins\"
                    --translations zh_CN
                    --no-system-d3d-compiler
                    --no-compiler-runtime
                    --no-opengl-sw
                    --force
                    --verbose 0
                    \"\${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${_target}>\"
                WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\"
            )
        ")
    endif()
endfunction()
