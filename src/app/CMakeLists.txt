project(DsEditorLite VERSION 0.0.0.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (APPLE)
    file(GLOB_RECURSE _src *.h *.cpp *.mm)
else ()
    file(GLOB_RECURSE _src *.h *.cpp)
endif ()

if (WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif ()

if (APPLE)
    set(PLATFORM_PROPERTY MACOSX_BUNDLE)
endif ()

add_executable(${PROJECT_NAME} ${PLATFORM_PROPERTY} ${_src} Resources/lite_res.qrc)

if (WIN32)
    set(RC_APPLICATION_NAME "Lite")
    set(RC_DESCRIPTION "DS Editor Lite")
    include("../../scripts/cmake/winrc.cmake")
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC .)
target_compile_definitions(${PROJECT_NAME} PRIVATE QT_MESSAGELOGCONTEXT)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Core5Compat)
find_package(OpenGL)
find_package(QWindowKit CONFIG REQUIRED)
find_package(language-manager CONFIG REQUIRED)
find_package(QasTool CONFIG REQUIRED)
find_package(opendspx CONFIG REQUIRED)
find_package(lyric-tab CONFIG REQUIRED)
find_package(talcs REQUIRED)
find_package(unofficial-tinyexpr CONFIG REQUIRED)
find_package(dsinfer CONFIG REQUIRED)

if (WIN32)
    find_package(QtWin32DirectManipulateHelper CONFIG REQUIRED)
endif ()

set(USE_LITE_CONTROLS OFF)

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC
        "$<BUILD_INTERFACE:${INTERVAL_TREE_INCLUDE_DIRS}>"
)
target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Core5Compat
        ${PLATFORM_LIB}
        talcs::Core talcs::Device talcs::Format talcs::Midi talcs::Dspx talcs::Widgets
        opendspx::opendspx
        language-manager::language-manager
        lyric-tab::lyric-tab
        QWindowKit::Widgets
        qtmediate::Widgets
        rmvpe-infer::rmvpe-infer some-infer::some-infer
        curve-util::curve-util
        unofficial::tinyexpr::tinyexpr
        dsinfer::dsinfer
)

if (WIN32)
    if (QtWin32DirectManipulateHelper_FOUND)
        message("-- Building with Direct Manipulation support on Windows")
        target_link_libraries(${PROJECT_NAME} PRIVATE
                QtWin32DirectManipulateHelper::Core
        )
        target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_DIRECT_MANIPULATION)
    else ()
        message("-- QtWin32DirectManipulateHelper is not found. "
                "Building without Direct Manipulation support on Windows")
    endif ()
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

if (ONNXRUNTIME_ENABLE_CUDA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ONNXRUNTIME_ENABLE_CUDA)
endif()

if (ONNXRUNTIME_ENABLE_DML)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ONNXRUNTIME_ENABLE_DML)
endif()

set_property(TARGET DeployedTargets APPEND PROPERTY TARGETS ${PROJECT_NAME})

# deploy Qt

# "QT_QMAKE_EXECUTABLE" is usually defined by QtCreator.
if(NOT DEFINED QT_QMAKE_EXECUTABLE)
    get_target_property(QT_QMAKE_EXECUTABLE Qt::qmake IMPORTED_LOCATION)
endif()

if(NOT EXISTS "${QT_QMAKE_EXECUTABLE}")
    message(WARNING "deploy_qt_runtime: Can't locate the QMake executable.")
    return()
endif()

cmake_path(GET QT_QMAKE_EXECUTABLE PARENT_PATH __qt_bin_dir)

if(WIN32)
    set(_qt_deploy_executable "windeployqt.exe")
    find_program(WINDEPLOYQT_EXE ${_qt_deploy_executable} HINTS "${__qt_bin_dir}")

    if(WINDEPLOYQT_EXE)
        message("-- Found ${_qt_deploy_executable}: ${WINDEPLOYQT_EXE}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                --verbose 0
                \"$<TARGET_FILE:${PROJECT_NAME}>\"
                COMMENT "Deploy Qt"
        )
    else()
        message(WARNING "${_qt_deploy_executable} not found, skipping deploying Qt")
    endif()
endif()

# copy dsinfer binaries
if (APPLE)
    # TODO: macOS
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            # copy dsinfer and synthrt libraries
            COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:dsinfer::dsinfer>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"

            COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:synthrt::synthrt>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"

            # copy dsinfer plugins
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${dsinfer_DIR}/../../plugins"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins"
    )

    install(DIRECTORY
            "${dsinfer_DIR}/../../plugins"
            DESTINATION .
    )

    install(FILES
            "$<TARGET_FILE:dsinfer::dsinfer>"
            "$<TARGET_FILE:synthrt::synthrt>"
            DESTINATION .
    )
endif ()

# copy phoneme dict
if (APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources
    )

    install(
            DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resources
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>
    )
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
            ${CMAKE_BINARY_DIR}/bin/Resources
    )

    install(
            DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resources
            DESTINATION .
    )
endif ()

find_package(cpp-pinyin)
# copy cpp-pinyin dict
if (APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${cpp-pinyin_DIR}/dict"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/dict"
    )

    install(DIRECTORY "${cpp-pinyin_DIR}/dict"
            DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources
    )
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${cpp-pinyin_DIR}/dict"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/dict"
    )

    install(DIRECTORY "${cpp-pinyin_DIR}/dict"
            DESTINATION .
    )
endif ()