project(TestProcessIntegration)

lite_add_test(${PROJECT_NAME}
    SOURCES
        test_main.cpp
        tst_process_integration.h
        tst_headless.cpp
        tst_mcp.cpp
        ../TestSupport/ProcessFixture.cpp
        ../../app/Bootstrap/AppHostMode.cpp
        ../../app/Bootstrap/SingleInstanceCoordinator.cpp
)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Network
    lite::AutomationWire
    lite::DsConnectorCore
    SndFile::sndfile
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 user32)
endif()
add_dependencies(${PROJECT_NAME} DsEditorLite DsConnectorLite)
lite_register_test(${PROJECT_NAME} CATEGORY process GUI TIMEOUT 300
    ARGS
        --editor "$<TARGET_FILE:DsEditorLite>"
        --connector "$<TARGET_FILE:DsConnectorLite>"
        --platform-plugins "$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>"
)
