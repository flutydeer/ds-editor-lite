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
# The main line's dependency root
# ----------------------------------
# The editor builds against the main line of synthrt together with wolf and otter. No public
# registry provides these three packages: the ports in scripts/vcpkg-ports provide them, and only
# scripts/vcpkg-manifest/vcpkg.json requests them. A dependency tree installed from another
# manifest lacks them, and find_package would then fail in a subdirectory with the generic vcpkg
# advice to extend CMAKE_PREFIX_PATH, which does not address the cause. The three packages are
# therefore checked once at this point, and a missing package produces a diagnostic that names the
# required installation.
foreach(_lite_required IN ITEMS synthrt wolf otter)
    find_package(${_lite_required} CONFIG QUIET)
    if(NOT ${_lite_required}_FOUND)
        list(APPEND _lite_missing_packages ${_lite_required})
    endif()
endforeach()
if(_lite_missing_packages)
    string(REPLACE ";" ", " _lite_missing_text "${_lite_missing_packages}")
    message(FATAL_ERROR
        "The editor requires the main line of synthrt together with wolf and otter, and the "
        "dependency tree in use lacks: ${_lite_missing_text}.\n"
        "\n"
        "The ports in scripts/vcpkg-ports provide these packages, and only "
        "scripts/vcpkg-manifest/vcpkg.json requests them. Install that manifest into the tree "
        "that the presets specify and configure against that tree:\n"
        "\n"
        "  CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \\\n"
        "  vcpkg install --x-manifest-root=scripts/vcpkg-manifest \\\n"
        "                --x-install-root=vcpkg/installed\n"
        "\n"
        "  cmake -S . -B <build dir> -DVCPKG_INSTALLED_DIR=\${CMAKE_SOURCE_DIR}/vcpkg/installed"
        " -DVCPKG_MANIFEST_MODE=OFF ...\n"
        "\n"
        "The installation requires Qt on CMAKE_PREFIX_PATH because qbreakpad depends on it. A "
        "build directory configured against a tree of the synthrt refactor line cannot be reused, "
        "because the two lines use different package layouts: the refactor line installs srt-* "
        "packages, and the main line installs plugins under plugins/<library>. A tree that still "
        "contains packages of the refactor line must be reinstalled from this manifest. "
        "docs/plans/synthrt-main-migration.md describes the migration.")
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