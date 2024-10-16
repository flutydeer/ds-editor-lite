include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)

parse_version(_version ${PROJECT_VERSION})

# No Icon
set(RC_ICON_COMMENT "//")
set(RC_ICON_PATH)
# set(RC_ICON_COMMENT "")
# set(RC_ICON_PATH "Resources/images/dspx.ico")

# Metadata
set(RC_VERSION ${_version_1},${_version_2},${_version_3},${_version_4})

if (NOT DEFINED RC_APPLICATION_NAME)
    set(RC_APPLICATION_NAME ${PROJECT_NAME})
endif ()

set(RC_VERSION_STRING ${PROJECT_VERSION})

if (NOT DEFINED RC_DESCRIPTION)
    set(RC_DESCRIPTION ${PROJECT_NAME})
endif ()

if (NOT DEFINED RC_COPYRIGHT)
    set(RC_COPYRIGHT "Copyright (C) 2022-2024 OpenVPI")
endif ()

# Generate rc file
set(_rc_path ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_res.rc)
configure_file(${CMAKE_CURRENT_LIST_DIR}/WinResource.rc.in ${_rc_path} @ONLY)

# Add source
target_sources(${PROJECT_NAME} PRIVATE ${_rc_path})
