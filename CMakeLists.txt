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
# This branch builds against the main line of synthrt plus wolf and otter, and those three do not
# come from any public registry: their ports live in scripts/vcpkg-overrides and are declared only
# by scripts/vcpkg-manifest/vcpkg.json. A tree installed any other way does not have them, and
# find_package then fails deep in a subdirectory with vcpkg's generic advice to add something to
# CMAKE_PREFIX_PATH -- which is not the problem and does not fix it.
#
# So they are looked for once, here, and a missing one says what to do. The probe is QUIET and its
# only side effect is that what it does find becomes visible slightly earlier.
foreach(_lite_required IN ITEMS synthrt wolf otter)
    find_package(${_lite_required} CONFIG QUIET)
    if(NOT ${_lite_required}_FOUND)
        list(APPEND _lite_missing_packages ${_lite_required})
    endif()
endforeach()
if(_lite_missing_packages)
    string(REPLACE ";" ", " _lite_missing_text "${_lite_missing_packages}")
    message(FATAL_ERROR
        "This branch needs the main line of synthrt together with wolf and otter, and the "
        "dependency tree in use does not carry: ${_lite_missing_text}.\n"
        "\n"
        "Those three are built from scripts/vcpkg-overrides, which only "
        "scripts/vcpkg-manifest/vcpkg.json names. Install that manifest into a tree of its own "
        "and point this configure at it:\n"
        "\n"
        "  CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \\\n"
        "  vcpkg install --x-manifest-root=scripts/vcpkg-manifest \\\n"
        "                --x-install-root=vcpkg/installed-main\n"
        "\n"
        "  cmake -S . -B <build dir> -DVCPKG_INSTALLED_DIR=\${CMAKE_SOURCE_DIR}/vcpkg/installed-main"
        " -DVCPKG_MANIFEST_MODE=OFF ...\n"
        "\n"
        "Qt must be on CMAKE_PREFIX_PATH for the install itself, or qbreakpad fails. A build "
        "directory configured against the refactor line's tree cannot be reused: the two lines "
        "both install lib/libsynthrt.so, so they cannot share an installed tree. See "
        "docs/plans/synthrt-main-migration.md.")
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