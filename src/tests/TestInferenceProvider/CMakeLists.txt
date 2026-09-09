function(add_inference_provider_test target)
    lite_add_test(${target} SOURCES
        test_main.cpp
        tst_inference_provider.h
        tst_inference_provider.cpp
        ../../app/Model/AppOptions/Options/InferenceOption.cpp
    )
    target_include_directories(${target} PRIVATE ../../app)
    target_link_libraries(${target} PRIVATE Qt6::Core)
    lite_register_test(${target} CATEGORY unit)
endfunction()

add_inference_provider_test(TestInferenceProviderDefault)
add_inference_provider_test(TestInferenceProviderCuda)
target_compile_definitions(TestInferenceProviderCuda PRIVATE ONNXRUNTIME_ENABLE_CUDA)
