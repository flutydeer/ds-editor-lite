# Repeat find_package in each consumer directory because imported targets are directory-scoped.

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
