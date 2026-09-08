project(TestAudioDecodingController)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Controller/AudioDecodingController.cpp
        ../../app/Controller/DocumentWorkflow/DocumentWorkflowController.h
        ../../app/Controller/DocumentWorkflow/DocumentWorkflowPathUtils.cpp
        ../../app/Controller/Tasks/DecodeAudioTask.cpp
        ../../app/Controller/Tasks/ResolveAudioPathTask.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
find_package(talcs REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    lite::Tasking
    talcs::Format
    talcs::Dspx
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
