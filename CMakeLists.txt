cmake_minimum_required(VERSION 3.17)

include(cmake/ProductMetadata.cmake)

project(lite
    VERSION ${LITE_PRODUCT_VERSION}
    LANGUAGES C CXX
    DESCRIPTION "${LITE_PRODUCT_NAME}"
    HOMEPAGE_URL "${LITE_PRODUCT_URL}"
)

# ----------------------------------
# Build Options
# ----------------------------------
option(LITE_BUILD_TESTS "Build test targets" OFF)
option(LITE_ENABLE_BREAKPAD "Enable breakpad for crash reporting" OFF)
option(LITE_ENABLE_FILE_LOG "Enable file logging (used by packaged builds)" OFF)
# Default is DML-only; opt in to CUDA per invocation with -D LITE_ENABLE_CUDA=ON
# (build-installer.ps1 -EnableCuda does the same). cmake/OrtRuntimeGate.cmake
# enforces the deployed flavors at build/install time, both directions.
option(LITE_ENABLE_CUDA "Enable CUDA execution provider (off for DML-only builds)"
    OFF)
option(LITE_INSTALL "Install the project" OFF)

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
configure_file(
    cmake/product-metadata.json.in
    "${CMAKE_CURRENT_BINARY_DIR}/product-metadata.json"
    @ONLY
)

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

if(LITE_INSTALL)
    if(MSVC)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "")
        include(InstallRequiredSystemLibraries)
    endif()
endif()