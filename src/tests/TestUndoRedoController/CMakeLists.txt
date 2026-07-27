project(TestUndoRedoController)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Controller/EditorViewController.cpp
        ../../app/Controller/UndoRedoController.cpp
        ../../app/Model/AppStatus/AppStatus.cpp
        ../../libs/ProjectModel/AppModel/LoopSettings.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        lite::Core
        lite::History
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
