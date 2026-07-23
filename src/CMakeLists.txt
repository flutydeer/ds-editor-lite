include(conf.cmake)

lite_init_buildsystem(LITE)

add_subdirectory(3rdparty)

qm_find_qt(Core)
qm_find_qt(Widgets)
qm_find_qt(Svg)

# After Qt is found: project libraries may link Qt (e.g. lite::Support -> Qt::Core).
add_subdirectory(libs)

add_subdirectory(app)

if(LITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(tools)

lite_finish_buildsystem()
