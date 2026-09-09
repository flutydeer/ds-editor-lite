add_library(EditorInferenceCore STATIC
    Tasks/InferTaskCommon.cpp
    Models/GenericInferModel.cpp
    Models/SpeakerMixValidator.cpp
)
target_include_directories(EditorInferenceCore PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_features(EditorInferenceCore PUBLIC cxx_std_20)
target_compile_definitions(EditorInferenceCore PUBLIC NOMINMAX QT_MESSAGELOGCONTEXT)
target_link_libraries(EditorInferenceCore PUBLIC
    Qt6::Core lite::ProjectModel lite::SynthrtEngine
    srt::synthrt srt::diffsinger dsinfer::dsinfer
)
