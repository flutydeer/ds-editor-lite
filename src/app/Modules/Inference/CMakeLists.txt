add_library(EditorInferenceCore STATIC
    Tasks/InferTaskCommon.cpp
    Models/GenericInferModel.cpp
    Models/InferInputBase.cpp
    Models/NoteInferenceSnapshot.cpp
    Models/SpeakerMixValidator.cpp
    Models/InferInputNote.cpp
    Tasks/Syllabification.cpp
    Utils/InferTaskHelper.cpp
    Utils/PitchRouting.cpp
    Utils/PitchSampling.cpp
    Utils/BasePitchCurve.cpp
    Utils/InferCacheUtils.cpp
    ../Extractors/AnalysisAudio.cpp
    ../Extractors/AudioSlicer.cpp
    ../Extractors/ExtractMidiTask.cpp
    ../Extractors/ExtractPitchTask.cpp
    ../Extractors/ExtractTask.cpp
)
set_target_properties(EditorInferenceCore PROPERTIES AUTOMOC ON)
target_include_directories(EditorInferenceCore PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_features(EditorInferenceCore PUBLIC cxx_std_20)
target_compile_definitions(EditorInferenceCore PUBLIC NOMINMAX QT_MESSAGELOGCONTEXT)
target_link_libraries(EditorInferenceCore PUBLIC
    Qt6::Core lite::ProjectModel lite::SynthrtEngine
    synthrt::synthrt dsinfer::dsinfer
    lite::Tasking talcs::Core talcs::Format SndFile::sndfile
)
