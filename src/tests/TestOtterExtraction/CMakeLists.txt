project(TestOtterExtraction)

# Runs the two extraction tasks against real otter executives, using the synthetic model fixtures
# of the otter tests. The otter build produces these fixtures (make-model-fixtures.py writes them
# to otter/build/cmake/fixtures); they are not stored in this tree. Set LITE_OTTER_FIXTURES here,
# or the environment variable of the same name at test time, to the directory that contains
# fixture-rmvpe and fixture-note. Without the fixtures the test checks nothing, and ctest reports
# the test as skipped instead of passed.
set(LITE_OTTER_FIXTURES "" CACHE PATH
    "otter model fixtures (fixture-rmvpe, fixture-note) for TestOtterExtraction; empty skips it")

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/Modules/Extractors/AnalysisAudio.cpp
        ../../app/Modules/Extractors/AudioSlicer.cpp
        ../../app/Modules/Extractors/ExtractMidiTask.cpp
        ../../app/Modules/Extractors/ExtractPitchTask.cpp
        ../../app/Modules/Extractors/ExtractTask.cpp
        ../../app/Modules/Inference/Utils/PitchSampling.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app ../../libs/SynthrtEngine)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(talcs REQUIRED)
find_package(SndFile CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::MusicBase
        lite::SynthrtEngine
        lite::Tasking
        talcs::Core
        talcs::Format
        SndFile::sndfile
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_OTTER_FIXTURES="${LITE_OTTER_FIXTURES}"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
