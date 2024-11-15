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
        svscraft::Widgets
        dsonnxinfer
        rmvpe-infer::rmvpe-infer some-infer::some-infer
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

set_property(TARGET DeployedTargets APPEND PROPERTY TARGETS ${PROJECT_NAME})

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