project(AutomationWire LANGUAGES CXX)

file(GLOB _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.inc
)

lite_add_library(AutomationWire STATIC SOURCES ${_src})
add_library(lite::AutomationWire ALIAS AutomationWire)

target_link_libraries(AutomationWire PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(AutomationWire PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(AutomationWire PREFIX lite/AutomationWire FORCE)
