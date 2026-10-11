#!/usr/bin/env bash
set -euo pipefail

deploy_tool="$1"
bundle_dir="$(cd "$2" && pwd)"

# Plugin manifests and models must be sealed as resources, not nested code.
# Relative links preserve the layout expected by third-party plugin loaders.
while IFS= read -r -d '' resource; do
    if [[ "$(file -b "$resource")" == *Mach-O* ]]; then
        continue
    fi
    relative_path="${resource#"$bundle_dir/Contents/"}"
    destination="$bundle_dir/Contents/Resources/runtime/$relative_path"
    mkdir -p "$(dirname "$destination")"
    mv "$resource" "$destination"
    link_target="Resources/runtime/$relative_path"
    remaining="$relative_path"
    while [[ "$remaining" == */* ]]; do
        link_target="../$link_target"
        remaining="${remaining#*/}"
    done
    ln -s "$link_target" "$resource"
done < <(find "$bundle_dir/Contents/MacOS" "$bundle_dir/Contents/PlugIns" -type f -print0)

arguments=("$bundle_dir" -verbose=1 -always-overwrite)
# Explicit plugin binaries make Qt use loader-relative dependency paths.
while IFS= read -r -d '' library; do
    arguments+=("-executable=$library")
done < <(find "$bundle_dir/Contents/PlugIns" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)

"$deploy_tool" "${arguments[@]}"
codesign --verify --deep --strict "$bundle_dir"
