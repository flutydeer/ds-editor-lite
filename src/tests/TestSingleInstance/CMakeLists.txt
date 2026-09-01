project(TestSingleInstance)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Bootstrap/SingleInstanceCoordinator.cpp
        ../../app/Bootstrap/SingleInstanceIdentity.cpp
        ../../app/Bootstrap/SingleInstanceProtocol.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
