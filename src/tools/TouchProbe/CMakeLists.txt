project(TouchProbe VERSION ${LITE_VERSION})

set(_src
    main.cpp
)

lite_add_tool(${PROJECT_NAME}
    SOURCES ${_src}
    INCLUDE_PRIVATE .
    FEATURES_PRIVATE cxx_std_20
    QT_LINKS_PRIVATE Core Gui Widgets
)

# Mirrors the application: when the helper is available the probe can register
# and unregister Direct Manipulation at runtime, which is the only way to see
# on real hardware which pointer types it swallows.
if(WIN32)
    find_package(QtWin32DirectManipulateHelper CONFIG QUIET)

    if(QtWin32DirectManipulateHelper_FOUND)
        qm_configure_target(${PROJECT_NAME}
            LINKS_PRIVATE QtWin32DirectManipulateHelper::Core
            DEFINES_PRIVATE WITH_DIRECT_MANIPULATION
        )
    endif()
endif()
