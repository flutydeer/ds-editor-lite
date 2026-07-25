project(TestVoiceContext)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        Stubs.cpp
        ../../libs/ProjectModel/AppModel/Clip.cpp
        ../../libs/ProjectModel/AppModel/Track.cpp
        ../../libs/ProjectModel/AppModel/TrackControl.cpp
        ../../libs/ProjectModel/AppModel/SpeakerMixData.cpp
        ../../libs/ProjectModel/Voice/SingerInfo.cpp
        ../../libs/ProjectModel/Voice/SpeakerInfo.cpp
        ../../libs/ProjectModel/Voice/LanguageInfo.cpp
        ../../libs/Core/IdGenerator.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${INTERVAL_TREE_INCLUDE_DIRS})

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::MusicBase
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
