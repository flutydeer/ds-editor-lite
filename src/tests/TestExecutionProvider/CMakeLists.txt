project(TestExecutionProvider)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

function(add_execution_provider_test target)
    lite_add_test(${target}
        SOURCES
        main.cpp
        ../../app/Modules/Inference/ExecutionProvider.cpp
    )

    target_include_directories(${target} PRIVATE
        ../../app
    )

    target_link_libraries(${target} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
    )

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_execution_provider_test(TestExecutionProviderDml)
add_execution_provider_test(TestExecutionProviderCuda)
target_compile_definitions(TestExecutionProviderCuda PRIVATE ONNXRUNTIME_ENABLE_CUDA)
