project(TestInputConversion)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        Stubs.cpp
        ../../libs/ProjectModel/InferenceData/InferPiece.h
        ../../app/Modules/Inference/Tasks/InferTaskCommon.cpp
        ../../app/Modules/Inference/Models/GenericInferModel.cpp
        ../../libs/ProjectModel/InferenceData/InferSpeakerMix.cpp
        ../../libs/ProjectModel/AppModel/SpeakerMixData.cpp
        ../../libs/ProjectModel/Voice/SpeakerInfo.cpp
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
        lite::MusicBase
        srt::synthrt
        srt::diffsinger
        dsinfer::dsinfer
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
