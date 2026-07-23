project(TestSpeakerMixValidation)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        Stubs.cpp
        ../../app/Model/InferenceData/InferPiece.h
        ../../app/Model/AppModel/SpeakerMixData.cpp
        ../../app/Modules/Inference/Models/GenericInferModel.cpp
        ../../app/Model/InferenceData/InferSpeakerMix.cpp
        ../../app/Modules/Inference/Models/SpeakerMixValidator.cpp
        ../../app/Modules/PackageManager/Models/SpeakerInfo.cpp
        ../../app/Modules/PackageManager/Models/SingerInfo.cpp
        ../../app/Modules/PackageManager/Models/LanguageInfo.cpp
        ../../app/Model/AppModel/Timeline.cpp
        ../../app/Model/AppModel/Tempo.cpp
        ../../app/Model/AppModel/TimeSignature.cpp
        ../../app/Utils/MusicTimeConverter.cpp
        ../../app/Modules/SynthrtEngine/SingerModelSession.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX QT_MESSAGELOGCONTEXT)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(synthrt CONFIG REQUIRED)
find_package(dsinfer CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        srt::synthrt
        srt::diffsinger
        dsinfer::dsinfer
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
