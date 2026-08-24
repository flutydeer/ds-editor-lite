project(TestProjectConverterAtomicWrite)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(srt-extract CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::ProjectConverters
    lite::ProjectModel
    Qt${QT_VERSION_MAJOR}::Core
    srt::extract
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
