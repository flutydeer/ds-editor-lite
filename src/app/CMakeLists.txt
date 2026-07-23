project(DsEditorLite VERSION ${LITE_VERSION})

if(APPLE)
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp *.mm)
else()
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets OpenGLWidgets Concurrent StateMachine LinguistTools
)
find_package(OpenGL)
find_package(QWindowKit CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(opendspx CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(cpp-pinyin CONFIG REQUIRED)
find_package(talcs REQUIRED)
find_package(unofficial-tinyexpr CONFIG REQUIRED)
find_package(dsinfer CONFIG REQUIRED)
find_package(synthrt CONFIG REQUIRED)
find_package(srt-audio CONFIG REQUIRED)
find_package(srt-extract CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)

set(_platform_links)
if(WIN32)
    list(APPEND _platform_links dwmapi.lib icu)
elseif(NOT APPLE)
    find_package(ICU COMPONENTS uc REQUIRED)
    list(APPEND _platform_links ICU::uc)
endif()

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)

lite_add_application(${PROJECT_NAME}
    QT_AUTOGEN
    SOURCES
        ${_src}
        Resources/lite_res.qrc
    INCLUDE_PRIVATE
        .
        ${INTERVAL_TREE_INCLUDE_DIRS}
    DEFINES_PRIVATE
        QT_MESSAGELOGCONTEXT
        NOMINMAX
        ONNXRUNTIME_ENABLE_CUDA
        ONNXRUNTIME_ENABLE_DML
    FEATURES_PRIVATE cxx_std_20
    QT_LINKS_PRIVATE
        Core
        Widgets
        Concurrent
        StateMachine
    LINKS_PRIVATE
        ${_platform_links}
        talcs::Core
        talcs::Device
        talcs::Format
        talcs::Midi
        talcs::Dspx
        talcs::Widgets
        opendspx::converter
        opendspx::serializer
        QWindowKit::Widgets
        qtmediate::Widgets
        curve-util::curve-util
        lite::ADT
        unofficial::tinyexpr::tinyexpr
        srt::synthrt
        srt::diffsinger
        srt::audio
        srt::extract
        SndFile::sndfile
        dsinfer::dsinfer
        re2::re2
)

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.dseditor.lite"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_INFO_STRING "DS Editor Lite"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2024"
    )
endif()

qm_add_win_rc(${PROJECT_NAME}
    NAME "${PROJECT_NAME}"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "DS Editor Lite"
    COPYRIGHT "Copyright (C) 2022-2024 OpenVPI"
)

# Enable Edit and Continue for MSVC Debug builds.
if(MSVC)
    qm_configure_target(${PROJECT_NAME}
        CCFLAGS $<$<CONFIG:Debug>:/ZI>
        LDFLAGS $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

if(LITE_ENABLE_BREAKPAD)
    find_package(qBreakpad CONFIG REQUIRED)
    qm_configure_target(${PROJECT_NAME}
        LINKS_PRIVATE qBreakpad::qBreakpad
        DEFINES_PRIVATE LITE_ENABLE_BREAKPAD
    )
endif()

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate)
    find_package(QtWin32DirectManipulateHelper CONFIG QUIET)
    if(QtWin32DirectManipulateHelper_FOUND)
        message(STATUS "Building with Direct Manipulation support on Windows")
        qm_configure_target(${PROJECT_NAME}
            LINKS_PRIVATE QtWin32DirectManipulateHelper::Core
            DEFINES_PRIVATE WITH_DIRECT_MANIPULATION
        )
    else()
        message(STATUS "Building without Direct Manipulation support on Windows")
    endif()
endif()

qt_add_translations(
    TARGETS ${PROJECT_NAME}
    TS_FILES Resources/translate/translation_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

lite_deploy_application(${PROJECT_NAME})
