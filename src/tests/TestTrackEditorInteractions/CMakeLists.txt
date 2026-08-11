project(TestTrackEditorInteractions)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Views/TrackEditor/ClipResizeUtils.cpp
        ../../app/UI/Views/TrackEditor/SingingClipPreviewLayout.cpp
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
