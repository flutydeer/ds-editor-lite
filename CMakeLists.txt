cmake_minimum_required(VERSION 3.17)

project(lite
    VERSION 0.0.0.1
    LANGUAGES C CXX
    DESCRIPTION "DS Editor Lite"
    HOMEPAGE_URL "https://github.com/flutydeer/ds-editor-lite"
)

# ----------------------------------
# Build Options
# ----------------------------------
option(LITE_BUILD_TESTS "Build test targets" OFF)
option(LITE_ENABLE_BREAKPAD "Enable breakpad for crash reporting" OFF)
option(LITE_INSTALL "Install the project" OFF)

# CMake silently ignores unknown -D arguments, so a stale APPLICATION_ENABLE_BREAKPAD
# would turn crash reporting off without any diagnostic. Honour it and warn instead.
if(DEFINED APPLICATION_ENABLE_BREAKPAD)
    message(WARNING "APPLICATION_ENABLE_BREAKPAD is deprecated, use LITE_ENABLE_BREAKPAD.")
    set(LITE_ENABLE_BREAKPAD ${APPLICATION_ENABLE_BREAKPAD})
endif()

# ----------------------------------
# CMake Settings
# ----------------------------------
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
elseif(MINGW)
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_IMPORT_LIBRARY_PREFIX "")
endif()

if(LITE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(LITE_VERSION ${PROJECT_VERSION})
set(LITE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(LITE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(LITE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------------
# Initialize QMSetup (Required)
# ----------------------------------
find_package(qmsetup REQUIRED)
qm_import(Filesystem)

if(NOT DEFINED QMSETUP_BUILD_DIR)
    set(QMSETUP_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
endif()

qm_init_directories()

# ----------------------------------
# Tests
# ----------------------------------
if(LITE_BUILD_TESTS)
    enable_testing()
endif()

# ----------------------------------
# Add source modules
# ----------------------------------
add_subdirectory(src)
