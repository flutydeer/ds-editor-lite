project(TestAutomationIdempotency)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    Qt${QT_VERSION_MAJOR}::Core
    Threads::Threads
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
