add_library(EditorPreferencesCore STATIC
    Options/AutomationOption.cpp
    Options/GeneralOption.cpp
    Options/InferenceOption.cpp
    Options/AppearanceOption.cpp
    ../../Automation/Mcp/McpClientConfiguration.cpp
    ../../Utils/ApplicationLocale.cpp
    ../../Utils/UiLanguageManager.cpp
)
set_target_properties(EditorPreferencesCore PROPERTIES AUTOMOC ON)
target_compile_features(EditorPreferencesCore PUBLIC cxx_std_20)
target_include_directories(EditorPreferencesCore PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(EditorPreferencesCore PUBLIC Qt6::Core Qt6::Widgets lite::Core)
if(LITE_ENABLE_CUDA)
    target_compile_definitions(EditorPreferencesCore PRIVATE ONNXRUNTIME_ENABLE_CUDA)
endif()
