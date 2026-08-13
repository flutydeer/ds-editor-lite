if(NOT DEFINED OUTPUT_PATH OR OUTPUT_PATH STREQUAL "")
    message(FATAL_ERROR "OUTPUT_PATH is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ProductMetadata.cmake")
configure_file("${CMAKE_CURRENT_LIST_DIR}/product-metadata.json.in" "${OUTPUT_PATH}" @ONLY)
