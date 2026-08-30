cmake_minimum_required(VERSION 3.17)

project(IcuWrapper VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core)

add_library(IcuWrapper)
add_library(IcuWrapper::IcuWrapper ALIAS IcuWrapper)

target_sources(IcuWrapper
    PRIVATE
        src/IcuWrapper.cpp
        include/IcuWrapper/IcuWrapper.h
)

target_include_directories(IcuWrapper PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(IcuWrapper PUBLIC cxx_std_17)
target_link_libraries(IcuWrapper PUBLIC Qt6::Core)
set_target_properties(IcuWrapper PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(IcuWrapper PRIVATE src/IcuWrapper_mac.mm)
    target_link_libraries(IcuWrapper PRIVATE "-framework Foundation")
elseif(WIN32)
    target_sources(IcuWrapper PRIVATE src/IcuWrapper_icu.cpp)
    target_link_libraries(IcuWrapper PRIVATE icu)
else()
    find_package(ICU 65 REQUIRED COMPONENTS uc)
    target_sources(IcuWrapper PRIVATE src/IcuWrapper_icu.cpp)
    target_link_libraries(IcuWrapper PRIVATE ICU::uc)
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(IcuWrapper
        PRIVATE ICU_WRAPPER_BUILD_SHARED
        INTERFACE ICU_WRAPPER_SHARED
    )
endif()

# Respect the parent project's test gate when built inside ds-editor-lite:
# include(CTest) leaves BUILD_TESTING=ON by default even when the host sets
# LITE_BUILD_TESTS=OFF. Standalone builds keep the standard BUILD_TESTING
# default (ON).
if(BUILD_TESTING AND (NOT DEFINED LITE_BUILD_TESTS OR LITE_BUILD_TESTS))
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_executable(IcuWrapperTests tests/IcuWrapperTests.cpp)
    set_target_properties(IcuWrapperTests PROPERTIES AUTOMOC ON)
    target_link_libraries(IcuWrapperTests PRIVATE IcuWrapper::IcuWrapper Qt6::Test)
    add_test(NAME IcuWrapperTests COMMAND IcuWrapperTests)
endif()

install(TARGETS IcuWrapper
    EXPORT IcuWrapperTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/IcuWrapper
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT IcuWrapperTargets
    FILE IcuWrapperTargets.cmake
    NAMESPACE IcuWrapper::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IcuWrapper
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/IcuWrapperConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/IcuWrapperConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IcuWrapper
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/IcuWrapperConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/IcuWrapperConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/IcuWrapperConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IcuWrapper
)
