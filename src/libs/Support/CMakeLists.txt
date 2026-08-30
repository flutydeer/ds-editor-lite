project(Support LANGUAGES CXX)

# Generic, domain-agnostic support utilities (string/math/system/version/linq
# helpers). Zero internal dependencies; a small compiled part (VersionUtils,
# MacOSUtils) makes this a static library rather than header-only.
file(GLOB _src CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(Support STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::Support ALIAS Support)

# Public headers use Qt Core (QString/QList/QSysInfo/QVersionNumber/...) and
# stdcorelib (VersionUtils.h exposes stdcorelib::support::VersionNumber).
find_package(stdcorelib CONFIG REQUIRED)
# IcuWrapper is vendored at src/3rdparty/icu-wrapper (added before libs/), so
# the IcuWrapper::IcuWrapper target is already defined — no find_package.
target_link_libraries(Support PUBLIC
    lite::Core
    Qt${QT_VERSION_MAJOR}::Core
    stdcorelib::stdcorelib
    IcuWrapper::IcuWrapper
)

# Export the shared lite build-include root so linking lite::Support puts the
# generated <lite/Support/*.h> forwarding headers on the consumer's path.
target_include_directories(Support PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(Support PREFIX lite/Support FORCE)
