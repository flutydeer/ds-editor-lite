project(TestHeadlessProcessIntegration)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../TestSupport/ProcessFixture.cpp
        ../../app/Bootstrap/AppHostMode.cpp
        ../../app/Bootstrap/SingleInstanceCoordinator.cpp
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    lite::DsConnectorCore
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 user32)
endif()
add_dependencies(${PROJECT_NAME} DsEditorLite)

lite_register_test(${PROJECT_NAME} CATEGORY process TIMEOUT 180
    ARGS --editor "$<TARGET_FILE:DsEditorLite>")
lite_register_test(${PROJECT_NAME} NAME TestHeadlessCrossHostIntegration CATEGORY gui GUI TIMEOUT 90
    ARGS --cross-host --editor "$<TARGET_FILE:DsEditorLite>"
        --platform-plugins "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
