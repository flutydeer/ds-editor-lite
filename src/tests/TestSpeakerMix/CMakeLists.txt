project(TestSpeakerMix)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(${PROJECT_NAME}
        main.cpp
        ../../app/Model/AppModel/SpeakerMixData.cpp
        ../../app/Modules/Inference/Models/InferSpeakerMix.cpp
        ../../app/Model/AppModel/Timeline.cpp
        ../../app/Model/AppModel/Tempo.cpp
        ../../app/Model/AppModel/TimeSignature.cpp
        ../../app/Utils/MusicTimeConverter.cpp
        ../../app/Modules/PackageManager/Models/SpeakerInfo.cpp
        ../../app/Utils/MusicTimeConverter.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
