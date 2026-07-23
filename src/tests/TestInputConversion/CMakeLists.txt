project(TestInputConversion)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        Stubs.cpp
        ../../app/Model/AppModel/InferPiece.h
        ../../app/Modules/Inference/Tasks/InferTaskCommon.cpp
        ../../app/Modules/Inference/Models/GenericInferModel.cpp
        ../../app/Model/AppModel/InferSpeakerMix.cpp
        ../../app/Model/AppModel/SpeakerMixData.cpp
        ../../app/Model/AppModel/Timeline.cpp
        ../../app/Model/AppModel/Tempo.cpp
        ../../app/Model/AppModel/TimeSignature.cpp
        ../../app/Utils/MusicTimeConverter.cpp
        ../../app/Modules/PackageManager/Models/SpeakerInfo.cpp
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
