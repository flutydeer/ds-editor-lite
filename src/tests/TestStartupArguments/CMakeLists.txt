project(TestStartupArguments)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Bootstrap/StartupArguments.cpp
        ../../app/Model/AppOptions/Options/AutomationOption.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
