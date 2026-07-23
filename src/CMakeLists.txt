include(conf.cmake)

lite_init_buildsystem(LITE)

add_subdirectory(3rdparty)
add_subdirectory(libs)

qm_find_qt(Core)
qm_find_qt(Widgets)
qm_find_qt(Svg)

add_subdirectory(app)

if(LITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_subdirectory(tools)

lite_finish_buildsystem()
