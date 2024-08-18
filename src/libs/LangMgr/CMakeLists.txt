cmake_minimum_required(VERSION 3.17)
project(LangMgr VERSION 0.0.0.1 LANGUAGES CXX)  # Language and G2p Manager

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ----------------------------------
# Build Options
# ----------------------------------
option(LangMgr_BUILD_TESTS "Build test cases" ON)
option(LangMgr_INSTALL "Install library" ON)

# ----------------------------------
# CMake Settings
# ----------------------------------
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    if (NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif ()
endif ()

if (LangMgr_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif ()
# ----------------------------------
# Main Project
# ----------------------------------
file(GLOB_RECURSE _src include/*.h src/*/*.h src/*/*.cpp)
add_library(${PROJECT_NAME} STATIC ${_src})

# ----------------------------------
# Tests
# ----------------------------------
if (LangMgr_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# ----------------------------------
# find dependencies
# ----------------------------------
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

find_package(cpp-pinyin CONFIG REQUIRED)
find_package(cpp-kana CONFIG REQUIRED)

# ----------------------------------
# link libraries
# ----------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        cpp-pinyin::cpp-pinyin
        cpp-kana::cpp-kana
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PUBLIC include)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/${PORT}/dict
        ${CMAKE_BINARY_DIR}/bin/dict
)