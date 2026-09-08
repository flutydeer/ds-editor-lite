project(TestAutomationAdmission)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/Public/AdmissionController.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    Qt${QT_VERSION_MAJOR}::Core
)

lite_register_test(${PROJECT_NAME} CATEGORY protocol)
