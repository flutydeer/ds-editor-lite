project(TestLocalizedText)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/Support/LocalizedTextUtils.cpp
)

# Tests already inherit the sync_include root from the parent; the header is
# reachable both via <lite/Support/...> and the direct lib source directory.
target_include_directories(${PROJECT_NAME} PRIVATE
        ../../libs/Support
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
