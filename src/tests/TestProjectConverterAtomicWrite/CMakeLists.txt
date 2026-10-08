project(TestProjectConverterAtomicWrite)

lite_add_test(${PROJECT_NAME} SOURCES
    main.cpp
    ../../app/Modules/Audio/AudioFilePublisher.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# No extraction library to link. On the main line an analyzer is a package contribution reached
# through otter, and AudioFilePublisher -- the one file from the app this compiles -- does not
# touch it; the dependency was the older line's extractor leaking in through a header.
target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::ProjectConverters
    lite::ProjectModel
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
