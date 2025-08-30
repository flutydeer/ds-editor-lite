# Detect OS and architecture
if(VCPKG_TARGET_IS_WINDOWS)
    set(_os "win")
    set(_ext "zip")
    set(_os_display_name "Windows")
elseif(VCPKG_TARGET_IS_LINUX)
    set(_os "linux")
    set(_ext "tgz")
    set(_os_display_name "Linux")
elseif(VCPKG_TARGET_IS_OSX)
    set(_os "osx")
    set(_ext "tgz")
    set(_os_display_name "macOS")
else()
    message(FATAL_ERROR "Unsupported operating system: ${VCPKG_TARGET_TRIPLET}")
endif()

if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    if(VCPKG_TARGET_IS_OSX)
        set(_arch "x86_64")
    else()
        set(_arch "x64")
    endif()
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "x86")
    set(_arch "x86")
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    if(VCPKG_TARGET_IS_LINUX)
        set(_arch "aarch64")
    else()
        set(_arch "arm64")
    endif()
else()
    message(FATAL_ERROR "Unsupported architecture: ${VCPKG_TARGET_ARCHITECTURE}")
endif()


include("${CMAKE_CURRENT_LIST_DIR}/versions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/hashes.cmake")

function(_copy_runtime_contents _src _dst)
    file(GLOB SOURCE_FILES "${_src}/*")
    file(MAKE_DIRECTORY "${_dst}")
    foreach(FILE IN LISTS SOURCE_FILES)
        file(COPY "${FILE}" DESTINATION "${_dst}")
    endforeach()
endfunction()

function(download_onnxruntime_from_github _ep _deploy_dst_dir)
    string(TOLOWER ${_ep} _ep)
    if(${_ep} STREQUAL cuda12)
        set(_full_version "gpu-cuda12-${_version_ort}")
        set(_full_version_zip "gpu-${_version_ort}")
    elseif(${_ep} STREQUAL cuda11)
        set(_full_version "gpu-${_version_ort}")
        set(_full_version_zip "gpu-${_version_ort}")
    else()
        set(_full_version ${_version_ort})
        set(_full_version_zip ${_version_ort})
    endif()

    set(_base_url "https://github.com/microsoft/onnxruntime/releases/download/v${_version_ort}")
    set(_name "onnxruntime-${_os}-${_arch}-${_full_version}")
    set(_name_zip "onnxruntime-${_os}-${_arch}-${_full_version_zip}")
    set(_download_filename "${_name}.${_ext}")
    set(_url "${_base_url}/${_download_filename}")

    message(STATUS "Downloading ONNX Runtime from ${_url}")

    lookup_onnxruntime_package_sha512("${_download_filename}" _file_hash)
    vcpkg_download_distfile(
            _downloaded_file_path
            URLS "${_url}"
            FILENAME "${_download_filename}"
            SHA512 "${_file_hash}"
    )

    set(_extract_dir "${CURRENT_BUILDTREES_DIR}/tmp_onnxruntime")

    file(REMOVE_RECURSE "${_extract_dir}")

    vcpkg_extract_archive(
            ARCHIVE "${_downloaded_file_path}"
            DESTINATION "${_extract_dir}"
    )

    file(COPY "${_extract_dir}/${_name_zip}/include" DESTINATION "${_deploy_dst_dir}")
    file(COPY "${_extract_dir}/${_name_zip}/lib" DESTINATION "${_deploy_dst_dir}")
    file(REMOVE_RECURSE ${_extract_dir})
endfunction()

function(download_onnxruntime_from_nuget _deploy_dst_dir)
    set(_url "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/${_version_ort}")
    set(_download_filename "Microsoft.ML.OnnxRuntime.DirectML.${_version_ort}.zip")

    message(STATUS "Downloading ONNX Runtime from ${_url}")

    lookup_onnxruntime_package_sha512("${_download_filename}" _file_hash)
    vcpkg_download_distfile(
            _downloaded_file_path_ort
            URLS "${_url}"
            FILENAME "${_download_filename}"
            SHA512 "${_file_hash}"
    )

    set(_extract_dir "${CURRENT_BUILDTREES_DIR}/tmp_onnxruntime")

    file(REMOVE_RECURSE "${_extract_dir}")

    message("Archive: ${_downloaded_file_path_ort}")
    vcpkg_extract_archive(
        ARCHIVE "${_downloaded_file_path_ort}"
        DESTINATION "${_extract_dir}"
    )

    file(COPY "${_extract_dir}/build/native/include" DESTINATION "${_deploy_dst_dir}")
    file(MAKE_DIRECTORY "${_deploy_dst_dir}/lib")

    _copy_runtime_contents("${_extract_dir}/runtimes/win-x64/native" "${_deploy_dst_dir}/lib")

    file(REMOVE_RECURSE "${_extract_dir}")
endfunction()

function(download_dml_from_nuget _deploy_dst_dir)
    set(_url "https://www.nuget.org/api/v2/package/Microsoft.AI.DirectML/${_version_dml}")
    set(_download_filename "Microsoft.AI.DirectML.${_version_dml}.zip")

    message(STATUS "Downloading DirectML from ${_url}")

    lookup_onnxruntime_package_sha512("${_download_filename}" _file_hash)
    vcpkg_download_distfile(
            _downloaded_file_path_dml
            URLS "${_url}"
            FILENAME "${_download_filename}"
            SHA512 "${_file_hash}"
    )

    set(_extract_dir "${CURRENT_BUILDTREES_DIR}/tmp_directml")

    file(REMOVE_RECURSE "${_extract_dir}")

    vcpkg_extract_archive(
        ARCHIVE "${_downloaded_file_path_dml}"
        DESTINATION "${_extract_dir}"
    )

    file(COPY "${_extract_dir}/include" DESTINATION "${_deploy_dst_dir}")
    file(MAKE_DIRECTORY "${_deploy_dst_dir}/lib")

    _copy_runtime_contents("${_extract_dir}/bin/x64-win" "${_deploy_dst_dir}/lib")

    file(REMOVE_RECURSE "${_extract_dir}")
endfunction()
