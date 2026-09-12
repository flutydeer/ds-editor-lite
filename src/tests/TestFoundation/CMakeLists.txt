project(TestFoundation)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_foundation.h
    tst_expected.cpp
    tst_localized_text.cpp
    tst_logging.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE lite::ADT lite::Support)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
