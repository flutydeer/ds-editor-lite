# ONNX Runtime CUDA flavor deployment gate.
#
# Single source of truth: LITE_ENABLE_CUDA. The vcpkg tree may or may not
# carry the cuda12 payload (--x-feature=cuda12); this gate makes the deploy
# decision explicit in both directions instead of silently shipping whatever
# the installed tree happens to contain:
#
#   * expect ON,  cuda/ absent -> FATAL_ERROR (explicit requirement unmet)
#   * expect OFF, cuda/ present-> remove + WARNING (stray payload from a
#                                vcpkg tree built with --x-feature=cuda12)
#   * consistent               -> STATUS line
#
# Usage (script mode, from LiteBuildApi.cmake):
#   cmake -D expect_cuda=<ON|OFF|1|0> -D cuda_dir=<path> -D phase=<build|install> -P OrtRuntimeGate.cmake

if(NOT DEFINED expect_cuda OR NOT DEFINED cuda_dir)
    message(FATAL_ERROR "OrtRuntimeGate.cmake: expect_cuda and cuda_dir are required")
endif()
if(NOT DEFINED phase)
    set(phase "build")
endif()

set(_tag "[ort-runtime-gate:${phase}]")
set(_fix_install
    "Fix: rerun vcpkg install with --x-feature=cuda12, or configure with LITE_ENABLE_CUDA=OFF")
set(_fix_drop
    "Fix: rerun vcpkg install without --x-feature=cuda12, then rebuild")

if(EXISTS "${cuda_dir}")
    if(expect_cuda)
        message(STATUS "${_tag} CUDA runtimes present as required: ${cuda_dir}")
    else()
        message(WARNING
            "${_tag} LITE_ENABLE_CUDA=OFF but CUDA runtimes exist; removing: ${cuda_dir}. ${_fix_drop}")
        file(REMOVE_RECURSE "${cuda_dir}")
    endif()
else()
    if(expect_cuda)
        message(FATAL_ERROR
            "${_tag} LITE_ENABLE_CUDA=ON but the CUDA runtimes are missing: ${cuda_dir}. ${_fix_install}")
    else()
        message(STATUS "${_tag} CUDA runtimes absent as configured (DML/CPU build)")
    endif()
endif()
