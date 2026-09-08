project(TestAnchorEditController)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/UI/Views/ClipEditor/AnchorEditor/AnchorEditController.cpp
        ../../app/UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    lite::ProjectModel
)

lite_register_test(${PROJECT_NAME} CATEGORY gui)
