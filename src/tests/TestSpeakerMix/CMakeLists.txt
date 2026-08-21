project(TestSpeakerMix)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/ProjectModel/AppModel/SpeakerMixData.cpp
        ../../libs/ProjectModel/InferenceData/InferSpeakerMix.cpp
        ../../libs/ProjectModel/Voice/SpeakerInfo.cpp
        ../../app/UI/Utils/SpeakerMixUtils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::MusicBase
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
