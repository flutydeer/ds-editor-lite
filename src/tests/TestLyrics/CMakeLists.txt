project(TestLyrics)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_lyrics.h
    tst_lyric_rules.cpp
    tst_syllabification.cpp
    tst_word_property_cascade.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorAutomationCore EditorInferenceCore)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
