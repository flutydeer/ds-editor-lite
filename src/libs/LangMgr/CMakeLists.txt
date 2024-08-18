cmake_minimum_required(VERSION 3.17)
project(LangMgr VERSION 0.0.0.1 LANGUAGES CXX)  # Language and G2p Manager

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ----------------------------------
# Build Options
# ----------------------------------
option(LANG_MANAGER_BUILD_STATIC "Build static library" OFF)
option(LANG_MANAGER_BUILD_TESTS "Build test cases" ON)
option(LANG_MANAGER_INSTALL "Install library" ON)

# ----------------------------------
# CMake Settings
# ----------------------------------
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

if (NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    if (NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif ()
endif ()

if (LANG_MANAGER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif ()
# ----------------------------------
# Main Project
# ----------------------------------
file(GLOB_RECURSE _src include/*.h src/*/*.h src/*/*.cpp)

if (LANG_MANAGER_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC LANG_MANAGER_STATIC)
else ()
    add_library(${PROJECT_NAME} SHARED)
endif ()
target_compile_definitions(${PROJECT_NAME} PRIVATE LANG_MANAGER_LIBRARY)

target_sources(${PROJECT_NAME} PRIVATE ${_src})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ----------------------------------
# Add platform specific
# ----------------------------------
if (WIN32)
    set(RC_DESCRIPTION "Language and G2p Manager.")
    set(RC_COPYRIGHT "Copyright (C) 2023-2024 wolfgitpr")
    include("cmake/winrc.cmake")
endif ()

# ----------------------------------
# Tests
# ----------------------------------
if (LANG_MANAGER_BUILD_TESTS)
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
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/${PORT}/dict
        ${CMAKE_BINARY_DIR}/bin/dict
)

# ----------------------------------
# install
# ----------------------------------
if (LANG_MANAGER_INSTALL)
    target_include_directories(${PROJECT_NAME} PUBLIC
            $<INSTALL_INTERFACE:"${CMAKE_CURRENT_SOURCE_DIR}/include">
    )

    install(TARGETS ${PROJECT_NAME}
            EXPORT ${PROJECT_NAME}Targets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
    )

    install(DIRECTORY include/${PROJECT_NAME}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    # Generate and install package config and version files
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
            ${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Config.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
            NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )
#
#    install(EXPORT ${PROJECT_NAME}Targets
#            FILE ${PROJECT_NAME}Targets.cmake
#            NAMESPACE ${PROJECT_NAME}::
#            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
#    )
endif ()