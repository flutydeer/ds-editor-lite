project(Core LANGUAGES CXX)

# Foundation runtime-identity infrastructure: process-unique id source
# (IdGenerator) and the UniqueObject base class. Decoupled from AppContext
# (plain Meyers singleton), so any layer may depend on it. Pure C++, no Qt.
file(GLOB _src CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(Core STATIC SOURCES ${_src})
add_library(lite::Core ALIAS Core)

# Export the shared lite build-include root so linking lite::Core puts the
# generated <lite/Core/*.h> forwarding headers on the consumer's path.
target_include_directories(Core PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(Core PREFIX lite/Core FORCE)
