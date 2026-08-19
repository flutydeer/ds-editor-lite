project(TestParamSupport)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/Model/Utils/ParamUtils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX QT_MESSAGELOGCONTEXT)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::ProjectModel
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
