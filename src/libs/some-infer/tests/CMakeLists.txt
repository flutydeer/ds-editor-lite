project(TestSome)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        some-infer::some-infer
        onnxruntime
)