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

function(lite_deploy_application _target)
    qm_import(Filesystem Deploy)

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
                ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/synthrt/G2pPackages
                $<TARGET_FILE_DIR:${_target}>/plugins/srt-g2p/G2pPackages
            COMMENT "Deploy application runtime dependencies"
        )
    elseif(APPLE)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                $<TARGET_FILE_DIR:dsinfer::srt-ds-infer>/../lib/plugins
                $<TARGET_BUNDLE_CONTENT_DIR:${_target}>/PlugIns
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/synthrt/G2pPackages
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
                ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/synthrt/G2pPackages
                $<TARGET_FILE_DIR:${_target}>/../lib/plugins/srt-g2p/G2pPackages
            COMMENT "Deploy application runtime dependencies"
        )
    endif()
endfunction()
