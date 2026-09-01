project(TestFillLyricTaggerOrder)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Model/AppOptions/Options/FillLyricOption.cpp
        ../../app/Modules/FillLyric/Utils/LyricRuleAutomationUtils.cpp
        ../../app/Modules/FillLyric/Utils/TaggerRuleOrder.cpp
        ../../app/Modules/FillLyric/Utils/TextTagger.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(re2 CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    lite::Support
    re2::re2
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
