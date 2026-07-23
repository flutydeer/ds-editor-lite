project(ADT LANGUAGES CXX)

# Header-only abstract data types (Expected, Overlappable(SerialList), Queue,
# Property, BitMaskFlag). Zero internal dependencies; only Qt/std plus the
# header-only interval-tree (provided globally by vcpkg).
add_library(ADT INTERFACE)
add_library(lite::ADT ALIAS ADT)

# Export the shared lite build-include root so linking lite::ADT puts the
# generated <lite/ADT/*.h> forwarding headers on the consumer's include path.
target_include_directories(ADT INTERFACE
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# Generate the public include tree: consumers include <lite/ADT/Xxx.h>.
# FORCE keeps the generated tree in sync while headers are still being added.
lite_sync_include(ADT PREFIX lite/ADT FORCE)
