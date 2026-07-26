project(TestAudioAnchor)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/ProjectModel/AppModel/Clip.cpp
        ../../libs/ProjectModel/AppModel/AudioClip.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX QT_MESSAGELOGCONTEXT)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::MusicBase
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
