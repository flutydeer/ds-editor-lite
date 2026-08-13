project(${LITE_EXECUTABLE_BASENAME} VERSION ${LITE_VERSION})

if(APPLE)
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp *.mm)
else()
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Network Concurrent StateMachine LinguistTools GuiPrivate ShaderTools
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
        ${LITE_GENERATED_INCLUDE_DIR}
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
        Network
        Concurrent
        StateMachine
    LINKS_PRIVATE
        Qt6::GuiPrivate
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
        lite::Core
        lite::ADT
        lite::Support
        lite::MusicBase
        lite::ProjectModel
        lite::History
        lite::SynthrtEngine
        lite::PackageManager
        lite::ProjectConverters
        lite::Language
        lite::Tasking
        lite::GUI
        unofficial::tinyexpr::tinyexpr
        srt::synthrt
        srt::diffsinger
        srt::audio
        srt::extract
        SndFile::sndfile
        dsinfer::dsinfer
        re2::re2
)

qt_add_shaders(${PROJECT_NAME} "editor_rhi_shaders"
    BASE
        UI/Views/Common/shaders
    PREFIX
        "/editor_rhi"
    FILES
        UI/Views/Common/shaders/solid.vert
        UI/Views/Common/shaders/solid.frag
        UI/Views/Common/shaders/texture.vert
        UI/Views/Common/shaders/texture.frag
)

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "${LITE_MACOS_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "${LITE_PRODUCT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_INFO_STRING "${LITE_PRODUCT_NAME}"
        MACOSX_BUNDLE_COPYRIGHT "${LITE_PRODUCT_COPYRIGHT}"
    )
endif()

qm_add_win_rc_enhanced(${PROJECT_NAME}
    NAME "${LITE_PRODUCT_NAME}"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "${LITE_PRODUCT_NAME}"
    COPYRIGHT "${LITE_PRODUCT_COPYRIGHT}"
    COMPANY "${LITE_PRODUCT_PUBLISHER}"
    INTERNAL_NAME "${LITE_EXECUTABLE_BASENAME}"
    ORIGINAL_FILENAME "${LITE_EXECUTABLE_BASENAME}.exe"
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
    SOURCE_TARGETS
        ${PROJECT_NAME}
        Core
        Support
        MusicBase
        ProjectModel
        History
        SynthrtEngine
        PackageManager
        ProjectConverters
        Language
        Tasking
        GUI
    TS_FILES Resources/translate/translation_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

lite_deploy_application(${PROJECT_NAME})
