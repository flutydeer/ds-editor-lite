project(MusicBase LANGUAGES CXX)

# Musical time and pitch value types for the document model.
file(GLOB _src CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(MusicBase STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::MusicBase ALIAS MusicBase)

# Qt Core for QString/QList/QtNumeric; lite::Support for ISerializable
# (TimeSignature serializes to JSON).
target_link_libraries(MusicBase PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Support
)

# Export the shared lite build-include root so linking lite::MusicBase puts the
# generated <lite/MusicBase/*.h> forwarding headers on the consumer's path.
target_include_directories(MusicBase PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(MusicBase PREFIX lite/MusicBase FORCE)
