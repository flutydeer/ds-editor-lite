# Locates dsinfer, which the main line of synthrt installs without a CMake package.
#
# On that line dsinfer ships a library, its headers and the ONNX driver plugin tree, but exports no
# config file, so consumers find it by hand. wolf and otter do the same thing for the same reason;
# this is here so the three places in this tree that need it agree about how.
#
# Defines, when found:
#   lite::dsinfer            an imported target carrying the library and its include directory
#   LITE_DSINFER_DRIVER_DIR  the plugin search path the ONNX driver lives under
#
# Defines LITE_HAS_DSINFER either way, so a caller can degrade rather than fail.

if(NOT TARGET lite::dsinfer)
    find_path(LITE_DSINFER_INCLUDE_DIR
        NAMES dsinfer/Inference/InferenceDriver.h
    )
    find_library(LITE_DSINFER_LIBRARY
        NAMES synthrt-dsinfer
    )

    if(LITE_DSINFER_INCLUDE_DIR AND LITE_DSINFER_LIBRARY)
        set(LITE_HAS_DSINFER ON)
        add_library(lite::dsinfer UNKNOWN IMPORTED)
        set_target_properties(lite::dsinfer PROPERTIES
            IMPORTED_LOCATION "${LITE_DSINFER_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LITE_DSINFER_INCLUDE_DIR}"
        )
        # A plugin search path holds one subdirectory per plugin, so this is the parent of the
        # onnx directory rather than the directory itself.
        get_filename_component(_lite_dsinfer_libdir "${LITE_DSINFER_LIBRARY}" DIRECTORY)
        set(LITE_DSINFER_DRIVER_DIR "${_lite_dsinfer_libdir}/plugins/dsinfer/inferencedrivers")
        message(STATUS "dsinfer: ${LITE_DSINFER_LIBRARY}")
    else()
        set(LITE_HAS_DSINFER OFF)
        message(FATAL_ERROR
            "dsinfer was not found. It comes from synthrt built with its onnx feature; without it "
            "there is no inference backend for the editor to run models on.")
    endif()
endif()
