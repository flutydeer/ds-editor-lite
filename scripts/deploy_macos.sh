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

# Qt may read a versioned dylib's install ID through its SONAME symlink.
# Keep that ID resolvable within the deployed private library directory.
while IFS= read -r -d '' library; do
    if ! otool -l "$library" | awk '
        $1 == "cmd" { rpath = ($2 == "LC_RPATH") }
        rpath && $1 == "path" && $2 == "@loader_path" { found = 1 }
        END { exit !found }
    '; then
        install_name_tool -add_rpath @loader_path "$library"
    fi
done < <(find "$bundle_dir/Contents/Frameworks/ffmpeg-builds" -type f -name '*.dylib' -print0)

arguments=("$bundle_dir" -verbose=1 -always-overwrite)
# Explicit plugin binaries make Qt use loader-relative dependency paths.
while IFS= read -r -d '' library; do
    arguments+=("-executable=$library")
done < <(find "$bundle_dir/Contents/PlugIns" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)

"$deploy_tool" "${arguments[@]}"
codesign --verify --deep --strict "$bundle_dir"
