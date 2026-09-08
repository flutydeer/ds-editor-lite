project(TestInferenceOption)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

function(add_inference_option_test target)
    lite_add_test(${target}
        SOURCES
        main.cpp
        ../../app/Model/AppOptions/Options/InferenceOption.cpp
    )

    target_include_directories(${target} PRIVATE
        ../../app
    )

    target_link_libraries(${target} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
    )

    lite_register_test(${target} CATEGORY unit)
endfunction()

add_inference_option_test(TestInferenceOptionDml)
add_inference_option_test(TestInferenceOptionCuda)
target_compile_definitions(TestInferenceOptionCuda PRIVATE ONNXRUNTIME_ENABLE_CUDA)
