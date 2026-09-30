project(TestAnalyzerPackages)

# The engine facade is what a host asks for its analysers through, so this builds against the same
# file the editor does instead of re-implementing the lookup beside it.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/SynthrtEngine/VoicebankCatalog.cpp
        ../../libs/SynthrtEngine/LanguageBridge.cpp
        ../../libs/SynthrtEngine/SingerPipeline.cpp
        ../../libs/SynthrtEngine/SynthrtBootstrap.cpp
        ../../libs/SynthrtEngine/SynthrtEngine.cpp
        ../../libs/Core/SingletonRegistry.cpp
        ../../libs/Support/VersionUtils.cpp
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(synthrt CONFIG REQUIRED)
find_package(wolf CONFIG REQUIRED)
find_package(otter CONFIG REQUIRED)
include(${LITE_CMAKE_DIR}/LiteDsinfer.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine ../../libs)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        synthrt::synthrt
        wolf::wolf
        otter::otter
        dsinfer::dsinfer
)

# The packages under test are written by the test itself, into a temporary directory it owns. What
# is being checked is which directory the engine looks in and what it does with what it finds
# there, so a fixture that has to be produced by another build -- and that would leave this test
# skipped on exactly the machine where it is needed -- would be the wrong trade.
#
# The plugin tree and ONNX Runtime are taken from where this build deploys them, next to the
# executable, which is also where the editor finds them. A run in a tree that has not been built
# that far yet skips rather than reporting a failure it cannot tell apart from a real one.
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
