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
            SKIP_INSTALL
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
            SKIP_INSTALL
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

    if(LITE_INSTALL AND _deploy_tool AND WIN32)
        install(CODE "
            execute_process(
                COMMAND \"${_deploy_tool}\"
                    --libdir \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}\"
                    --plugindir \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}/plugins\"
                    --translations zh_CN
                    --no-system-d3d-compiler
                    --no-compiler-runtime
                    --no-opengl-sw
                    --force
                    --verbose 0
                    \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}/$<TARGET_FILE_NAME:${_target}>\"
                WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${LITE_INSTALL_RUNTIME_DIR}\"
            )
        ")
    endif()
endfunction()
