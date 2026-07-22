project(TestUiLanguage)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/Utils/UiLanguageManager.cpp
        ../../app/Model/AppOptions/Options/GeneralOption.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
