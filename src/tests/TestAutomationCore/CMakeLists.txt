project(TestAutomationCore)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/AutomationTypes.cpp
        ../../app/Automation/OperationCatalog.cpp
        ../../app/Automation/IdempotencyStore.cpp
        ../../app/Automation/DocumentSession.cpp
        ../../app/Automation/DocumentSessionResolver.cpp
        ../../app/Automation/SingleWindowContext.cpp
        ../../app/Automation/AutomationDispatcher.cpp
        ../../app/Automation/EditorAutomationFacade.cpp
        ../../app/Automation/CoreRuntime.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::ADT
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
