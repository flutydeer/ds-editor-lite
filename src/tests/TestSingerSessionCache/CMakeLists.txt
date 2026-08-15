project(TestSingerSessionCache)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    lite::ProjectModel
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
