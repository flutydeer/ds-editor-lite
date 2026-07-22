project(TestVoiceContext)

set(CMAKE_AUTOMOC ON)

add_executable(${PROJECT_NAME}
        main.cpp
        Stubs.cpp
        ../../app/Model/AppModel/Clip.cpp
        ../../app/Model/AppModel/Track.cpp
        ../../app/Model/AppModel/TrackControl.cpp
        ../../app/Model/AppModel/SpeakerMixData.cpp
        ../../app/Modules/PackageManager/Models/SingerInfo.cpp
        ../../app/Modules/PackageManager/Models/SpeakerInfo.cpp
        ../../app/Modules/PackageManager/Models/LanguageInfo.cpp
        ../../app/Utils/IdGenerator.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${INTERVAL_TREE_INCLUDE_DIRS})

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
