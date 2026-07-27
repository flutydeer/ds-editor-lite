project(History LANGUAGES CXX)

# Undo/redo history: the action interface (IAction), action sequences, the
# HistoryManager singleton and the focus-transition value types (HistoryFocus).
# App-agnostic; the undo/redo policy that depends on app-runtime state lives in
# the app's UndoRedoController. Consumed as <lite/History/Xxx.h>.
file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(History STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::History ALIAS History)

target_link_libraries(History PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Core
    lite::Support
)

# Export the shared lite build-include root so linking lite::History puts the
# generated <lite/History/*.h> forwarding headers on the consumer's path.
target_include_directories(History PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(History PREFIX lite/History FORCE)
