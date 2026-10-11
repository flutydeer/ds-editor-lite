# dsinfer comes from synthrt's onnx feature and is required: without it there is no inference
# backend. An imported target is visible only in the directory that created it, so every directory
# that links it calls find_package here; only the report is made once.

find_package(dsinfer CONFIG QUIET)

if(NOT TARGET dsinfer::dsinfer)
    message(FATAL_ERROR
        "dsinfer was not found. It comes from synthrt built with its onnx feature; without it "
        "there is no inference backend for the editor to run models on.")
endif()

get_property(_lite_dsinfer_reported GLOBAL PROPERTY LITE_DSINFER_REPORTED)
if(NOT _lite_dsinfer_reported)
    message(STATUS "dsinfer: ${dsinfer_DIR}")
    set_property(GLOBAL PROPERTY LITE_DSINFER_REPORTED ON)
endif()
