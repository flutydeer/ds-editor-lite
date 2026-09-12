project(${LITE_EXECUTABLE_BASENAME} VERSION ${LITE_VERSION})

if(APPLE)
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp *.mm)
else()
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp)
endif()

if(MSVC)
    set_source_files_properties(
        Automation/Public/PublicAutomationRegistry.cpp
        PROPERTIES COMPILE_OPTIONS "/bigobj"
    )
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Network HttpServer Concurrent StateMachine LinguistTools GuiPrivate ShaderTools
)
find_package(OpenGL)
find_package(QWindowKit CONFIG REQUIRED)
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

add_subdirectory(Automation)
add_subdirectory(Modules/Inference)
add_subdirectory(Model/AppOptions)
add_subdirectory(UI)
foreach(_component IN ITEMS
        EditorAutomationCore EditorInferenceCore EditorPreferencesCore EditorGuiCore)
    get_target_property(_component_sources ${_component} SOURCES)
    get_target_property(_component_directory ${_component} SOURCE_DIR)
    foreach(_source IN LISTS _component_sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE
            BASE_DIR "${_component_directory}")
        list(REMOVE_ITEM _src "${_absolute}")
        # QObject headers must belong to the same target as their implementation.
        string(REGEX REPLACE "\\.cpp$" ".h" _header "${_absolute}")
        list(REMOVE_ITEM _src "${_header}")
    endforeach()
endforeach()

list(REMOVE_ITEM _src "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
add_library(EditorRuntime OBJECT ${_src})
set_target_properties(EditorRuntime PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
target_include_directories(EditorRuntime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${LITE_BUILD_INCLUDE_DIR}" ${INTERVAL_TREE_INCLUDE_DIRS})
target_compile_features(EditorRuntime PUBLIC cxx_std_20)
target_compile_definitions(EditorRuntime PUBLIC QT_MESSAGELOGCONTEXT NOMINMAX ONNXRUNTIME_ENABLE_DML)
target_link_libraries(EditorRuntime PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::HttpServer
        Qt6::Concurrent
        Qt6::StateMachine
        EditorAutomationCore
        EditorInferenceCore
        EditorPreferencesCore
        EditorGuiCore
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
        lite::AutomationWire
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

lite_add_application(${PROJECT_NAME}
    QT_AUTOGEN
    SOURCES main.cpp
    LINKS_PRIVATE EditorRuntime
)

qt_add_shaders(EditorRuntime "editor_rhi_shaders"
    BASE
        UI/Views/Common/shaders
    PREFIX
        "/editor_rhi"
    FILES
        UI/Views/Common/shaders/solid.vert
        UI/Views/Common/shaders/solid.frag
        UI/Views/Common/shaders/texture.vert
        UI/Views/Common/shaders/texture.frag
        UI/Views/Common/shaders/scene.vert
        UI/Views/Common/shaders/scene.frag
        UI/Views/Common/shaders/overlay.vert
        UI/Views/Common/shaders/overlay.frag
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

# Edit and Continue is incompatible with native static coverage instrumentation.
if(MSVC AND NOT LITE_TEST_COVERAGE)
    qm_configure_target(EditorRuntime
        CCFLAGS $<$<CONFIG:Debug>:/ZI>
    )
    qm_configure_target(${PROJECT_NAME}
        LDFLAGS $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

if(LITE_ENABLE_BREAKPAD)
    find_package(qBreakpad CONFIG REQUIRED)
    target_link_libraries(EditorRuntime PUBLIC qBreakpad::qBreakpad)
    target_compile_definitions(EditorRuntime PUBLIC LITE_ENABLE_BREAKPAD)
endif()

if(LITE_ENABLE_FILE_LOG)
    target_compile_definitions(EditorRuntime PUBLIC LITE_ENABLE_FILE_LOG)
endif()

if(LITE_ENABLE_CUDA)
    target_compile_definitions(EditorRuntime PUBLIC ONNXRUNTIME_ENABLE_CUDA)
endif()

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate)
    find_package(QtWin32DirectManipulateHelper CONFIG QUIET)
    if(QtWin32DirectManipulateHelper_FOUND)
        message(STATUS "Building with Direct Manipulation support on Windows")
        target_link_libraries(EditorRuntime PUBLIC QtWin32DirectManipulateHelper::Core)
        target_compile_definitions(EditorRuntime PUBLIC WITH_DIRECT_MANIPULATION)
    else()
        message(STATUS "Building without Direct Manipulation support on Windows")
    endif()
endif()

qt_add_translations(
    TARGETS EditorRuntime
    SOURCE_TARGETS
        ${PROJECT_NAME}
        EditorRuntime
        EditorAutomationCore
        EditorInferenceCore
        EditorPreferencesCore
        EditorGuiCore
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
    MERGE_QT_TRANSLATIONS
    QT_TRANSLATION_CATALOGS qtbase
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

lite_deploy_application(${PROJECT_NAME})
