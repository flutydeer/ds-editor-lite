project(TestPersistence)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Utils/FileRevision.cpp
        ../../app/Model/AppOptions/AppOptions.cpp
        ../../app/Model/AppOptions/Options/AppearanceOption.cpp
        ../../app/Model/AppOptions/Options/AudioOption.cpp
        ../../app/Model/AppOptions/Options/DeveloperOption.cpp
        ../../app/Model/AppOptions/Options/FillLyricOption.cpp
        ../../app/Model/AppOptions/Options/G2pLanguageOption.cpp
        ../../app/Model/AppOptions/Options/GeneralOption.cpp
        ../../app/Model/AppOptions/Options/InferenceOption.cpp
        ../../app/Model/AppOptions/Options/WindowOption.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    lite::Core
    lite::ADT
    lite::Support
    lite::ProjectModel
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
