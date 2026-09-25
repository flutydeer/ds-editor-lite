project(TestPenInput)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Views/Common/EditorPenStroke.cpp
        ../../app/UI/Views/Common/EditorPenTarget.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Test)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Test
        lite::MusicBase
        lite::ProjectModel
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
