project(TestScrollBarInterplay)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Views/Common/EditorRhiScrollBarController.cpp
        ../../app/UI/Views/Common/EditorViewportAnimation.cpp
        ../../app/UI/Views/Common/EditorViewportController.cpp
        ../../app/UI/Views/Common/TimeGraphicsScene.cpp
        ../../app/UI/Views/ClipEditor/PianoRoll/PianoRollCoord.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        lite::GUI
)

lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
