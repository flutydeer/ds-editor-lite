project(TestAutomationArchitecture)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LITE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
