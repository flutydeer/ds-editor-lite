project(TestBootstrap)

lite_add_test(${PROJECT_NAME}
    SOURCES
        test_main.cpp
        tst_bootstrap.h
        tst_arguments.cpp
        tst_single_instance.cpp
        ../../app/Bootstrap/AppHostMode.cpp
        ../../app/Bootstrap/StartupArguments.cpp
        ../../app/Bootstrap/SingleInstanceCoordinator.cpp
        ../../app/Bootstrap/SingleInstanceIdentity.cpp
        ../../app/Bootstrap/SingleInstanceProtocol.cpp
        ../../app/Model/AppOptions/Options/AutomationOption.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Network)
lite_register_test(${PROJECT_NAME} CATEGORY protocol)
