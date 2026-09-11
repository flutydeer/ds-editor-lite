project(TestAudioSlicer)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/Modules/Extractors/AudioSlicer.cpp
        ../../app/Modules/Extractors/AnalysisAudio.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(talcs REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        talcs::Core
        talcs::Format
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
