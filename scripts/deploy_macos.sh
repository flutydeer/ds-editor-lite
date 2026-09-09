#!/usr/bin/env bash
set -euo pipefail

deploy_tool="$1"
bundle_dir="$2"
ffmpeg_private_dir="$3"

arguments=("$bundle_dir" -verbose=1 -always-overwrite "-libpath=$ffmpeg_private_dir")
# Explicit plugin binaries make Qt use loader-relative dependency paths.
while IFS= read -r -d '' library; do
    arguments+=("-executable=$library")
done < <(find "$bundle_dir/Contents/PlugIns" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)

"$deploy_tool" "${arguments[@]}"
codesign --verify --deep --strict "$bundle_dir"
