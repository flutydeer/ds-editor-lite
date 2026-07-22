# Language Configurations
function(_lite_common_configure_target _target)
    qm_configure_target(${_target} FEATURES cxx_std_20)
endfunction()

set(LITE_POST_CONFIGURE_COMMANDS _lite_common_configure_target)

# Dependency Configurations
set(TALCS_DSPX ON)
set(TALCS_WIDGETS ON)

# Project Constants
set(LITE_CMAKE_DIR "${LITE_SOURCE_DIR}/cmake")

# Include Build Helpers
set(QM_BUILD_REPO_HELPERS_FUNCTION_PREFIX lite)
include(${LITE_CMAKE_DIR}/QMBuildRepoHelpers.cmake)
include(${LITE_CMAKE_DIR}/LiteBuildApi.cmake)