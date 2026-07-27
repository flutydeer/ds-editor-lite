project(TestDocumentWorkflow)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/ProjectModel/AppModel/LoopSettings.cpp
        ../../app/Model/AppStatus/AppStatus.cpp
        ../../app/Utils/ConditionalTransition.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core StateMachine)

target_link_libraries(${PROJECT_NAME} PRIVATE
        lite::Core
        lite::History
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::StateMachine
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
