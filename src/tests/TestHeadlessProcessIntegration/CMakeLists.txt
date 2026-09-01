project(TestHeadlessProcessIntegration)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)
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

add_test(NAME ${PROJECT_NAME}
    COMMAND ${PROJECT_NAME}
        "$<TARGET_FILE:DsEditorLite>"
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>"
)
set_tests_properties(${PROJECT_NAME} PROPERTIES
    RESOURCE_LOCK editor_primary
    RUN_SERIAL TRUE
    TIMEOUT 120
)
