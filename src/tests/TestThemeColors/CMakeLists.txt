project(TestThemeColors)

set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
        main.cpp
        ../../app/UI/Utils/ColorUtils.cpp
        ../../app/UI/Utils/Theme/ThemeColorResolver.cpp
        ../../app/UI/Utils/Theme/ThemeLoader.cpp
        ../../app/Resources/lite_res.qrc
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
