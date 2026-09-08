project(TestAudioAssetResolution)

lite_add_test(${PROJECT_NAME}
    SOURCES
        ../../app/Controller/Tasks/ComputeAudioHashTask.cpp
        ../../app/Controller/Tasks/ResolveAudioPathTask.cpp
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    lite::Tasking
    Qt${QT_VERSION_MAJOR}::Core
)

lite_register_test(${PROJECT_NAME} CATEGORY workflow)
