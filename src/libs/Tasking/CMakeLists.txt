project(Tasking LANGUAGES CXX)

# TaskManager uses QtConcurrent; Concurrent is not found at the top level.
qm_find_qt(Concurrent)

# Generic asynchronous task-execution runtime: the Task/UniqueObject base, the
# QThreadPool-backed TaskManager (a leaky Meyers singleton, drained by AppContext
# during teardown), TaskQueue and DelayTimer. Domain-agnostic; concrete task
# subclasses live in app. Included as <lite/Tasking/Xxx.h>.
file(GLOB _src CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(Tasking STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::Tasking ALIAS Tasking)

target_link_libraries(Tasking PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Concurrent
    lite::Core
    lite::Support
    lite::ADT
)

# Export the shared lite build-include root so linking lite::Tasking puts the
# generated <lite/Tasking/*.h> forwarding headers on the consumer's path.
target_include_directories(Tasking PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(Tasking PREFIX lite/Tasking FORCE)
