project(TestAutomationWire)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::AutomationWire
    Qt${QT_VERSION_MAJOR}::Core
)

lite_register_test(${PROJECT_NAME} CATEGORY protocol)
