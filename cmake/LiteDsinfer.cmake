# Locates dsinfer, the inference backend the editor runs its models on.
#
# dsinfer is built by synthrt's onnx feature and installs its own CMake package beside synthrt's,
# so it is found the ordinary way and linked as dsinfer::dsinfer. It is required: without it there
# is no backend for the editor to run models on, so a tree that lacks it is refused here rather
# than deep inside a subdirectory.
#
# Included from every directory that links it, and find_package runs in each: an imported target
# is visible only in the directory that created it, so a single guarded call would leave every
# other directory's target_link_libraries pointing at a name it cannot see. Only the report is
# made once.

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
