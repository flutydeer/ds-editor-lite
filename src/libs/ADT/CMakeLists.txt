project(ADT LANGUAGES CXX)

# Header-only data structures and value types.
add_library(ADT INTERFACE)
add_library(lite::ADT ALIAS ADT)

# Export the shared lite build-include root so linking lite::ADT puts the
# generated <lite/ADT/*.h> forwarding headers on the consumer's include path.
target_include_directories(ADT INTERFACE
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(ADT PREFIX lite/ADT FORCE)
