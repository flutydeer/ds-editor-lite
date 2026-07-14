project(TestDocumentWorkflow)

set(CMAKE_AUTOMOC ON)

add_executable(${PROJECT_NAME}
        main.cpp
        ../../app/Model/AppModel/LoopSettings.cpp
        ../../app/Model/AppStatus/AppStatus.cpp
        ../../app/Modules/History/ActionSequence.cpp
        ../../app/Modules/History/HistoryManager.cpp
        ../../app/Utils/ConditionalTransition.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core StateMachine)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::StateMachine
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
