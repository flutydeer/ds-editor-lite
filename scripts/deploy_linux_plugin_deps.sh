#!/usr/bin/env bash
# Usage: deploy_linux_plugin_deps.sh <deploy-lib-dir> <source-lib-dir>...
# Copy transitive plugin dependencies, searching source directories in order.
# Read ELF NEEDED entries with objdump so discovery does not depend on the current RPATH.
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $(basename "$0") <deploy-lib-dir> <source-lib-dir>..." >&2
    exit 2
fi

DEPLOY_DIR="$1"
shift
SOURCE_DIRS=("$@")

if [ ! -d "$DEPLOY_DIR" ]; then
    echo "deploy_linux_plugin_deps: no deploy directory at $DEPLOY_DIR" >&2
    exit 0
fi

needed_of() {
    objdump -p "$1" 2>/dev/null | awk '/NEEDED/ { print $2 }'
}

find_source() {
    local name="$1" dir
    for dir in "${SOURCE_DIRS[@]}"; do
        if [ -e "$dir/$name" ]; then
            printf '%s\n' "$dir/$name"
            return 0
        fi
    done
    return 1
}

# Preserve the symlink chain from the soname to the versioned library.
copy_with_links() {
    local source="$1" name target real
    name="$(basename "$source")"
    real="$(readlink -f "$source")"
    cp -f "$real" "$DEPLOY_DIR/$(basename "$real")"
    target="$source"
    while [ -L "$target" ]; do
        local link
        link="$(readlink "$target")"
        ln -sf "$(basename "$link")" "$DEPLOY_DIR/$(basename "$target")"
        target="$(dirname "$target")/$link"
    done
    if [ ! -e "$DEPLOY_DIR/$name" ]; then
        cp -f "$real" "$DEPLOY_DIR/$name"
    fi
}

# Store source SHA256 digests; deployed files may have their RPATH rewritten.
RECORD="$DEPLOY_DIR/.plugin-deps"
touch "$RECORD"

recorded() {
    awk -v name="$1" '$1 == name { $1 = ""; sub(/^ /, ""); print; exit }' "$RECORD"
}

record() {
    local kept
    kept="$(awk -v name="$1" '$1 != name' "$RECORD")"
    { [ -n "$kept" ] && printf '%s\n' "$kept"; printf '%s %s\n' "$1" "$2"; } > "$RECORD"
}

copied=0
round=0
while :; do
    round=$((round + 1))
    added=0
    while IFS= read -r -d '' object; do
        while read -r name; do
            [ -n "$name" ] || continue
            source="$(find_source "$name")" || continue
            # Compare source content, since deployment rewrites the destination RPATH and
            # timestamps.
            signature="$(sha256sum "$(readlink -f "$source")" | awk '{print $1}')"
            if [ -e "$DEPLOY_DIR/$name" ] && [ "$(recorded "$name")" = "$signature" ]; then
                continue
            fi
            copy_with_links "$source"
            record "$name" "$signature"
            added=$((added + 1))
            copied=$((copied + 1))
        done < <(needed_of "$object")
    done < <(find "$DEPLOY_DIR" -type f \( -name '*.so' -o -name '*.so.*' \) -print0)

    [ "$added" -eq 0 ] && break
    # Bound traversal of broken symlink cycles.
    if [ "$round" -ge 16 ]; then
        echo "deploy_linux_plugin_deps: stopped after $round rounds" >&2
        break
    fi
done

echo "deploy_linux_plugin_deps: copied $copied librar$([ "$copied" = 1 ] && echo y || echo ies)"
