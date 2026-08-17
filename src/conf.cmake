# Language Configurations
function(_lite_common_configure_target _target)
    qm_configure_target(${_target} FEATURES cxx_std_20)
endfunction()

set(LITE_POST_CONFIGURE_COMMANDS _lite_common_configure_target)

# Emit build metadata
set(LITE_BUILD_INFO_HEADER_PATH lite/BuildInfo.h)
set(LITE_BUILD_INFO_HEADER_PREFIX LITE)

# Install the application's debug symbols
set(LITE_INSTALL_PDB ON)

# Dependency Configurations
set(TALCS_DSPX ON)
set(TALCS_WIDGETS ON)

# Project Constants
set(LITE_CMAKE_DIR "${LITE_SOURCE_DIR}/cmake")

# Include Build Helpers
qm_import(private/BuildSystem)
qm_setup_build_repo_helpers(lite)
include(${LITE_CMAKE_DIR}/LiteBuildApi.cmake)