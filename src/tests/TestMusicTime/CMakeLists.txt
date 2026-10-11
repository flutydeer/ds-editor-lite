project(TestMusicTime)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_music_time.h
    tst_music_timeline.cpp
    tst_audio_anchor.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE lite::MusicBase lite::ProjectModel)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
