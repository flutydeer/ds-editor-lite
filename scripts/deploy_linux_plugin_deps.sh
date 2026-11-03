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

# Track only files this script deploys. Older records also contain a source digest.
RECORD="$DEPLOY_DIR/.plugin-deps"
touch "$RECORD"
declare -A managed=() used=() visited=()

# Include every symlink and its final versioned library in ownership tracking.
chain_files() {
    local path="$1" link
    while :; do
        basename "$path"
        [ -L "$path" ] || break
        link="$(readlink "$path")"
        case "$link" in
            /*) path="$link" ;;
            *) path="$(dirname "$path")/$link" ;;
        esac
    done
}

while read -r name _; do
    case "$name" in
        ''|.|..|*/*) echo "Invalid plugin dependency record: $name" >&2; exit 1 ;;
    esac
    while read -r file; do managed["$file"]=1; done < <(chain_files "$DEPLOY_DIR/$name")
done < "$RECORD"

copy_with_links() {
    local source="$1" target link real
    real="$(readlink -f "$source")"
    cp -f "$real" "$DEPLOY_DIR/$(basename "$real")"
    target="$source"
    while [ -L "$target" ]; do
        link="$(readlink "$target")"
        ln -sf "$(basename "$link")" "$DEPLOY_DIR/$(basename "$target")"
        case "$link" in
            /*) target="$link" ;;
            *) target="$(dirname "$target")/$link" ;;
        esac
    done
    while read -r file; do used["$file"]=1; done < <(chain_files "$source")
}

# Old managed dependencies cannot be graph roots: they would keep one another alive forever.
queue=()
while IFS= read -r -d '' object; do
    name="${object#"$DEPLOY_DIR/"}"
    [ -n "${managed[$name]:-}" ] || queue+=("$object")
done < <(find "$DEPLOY_DIR" -type f \( -name '*.so' -o -name '*.so.*' \) -print0)
if [ -d "$DEPLOY_DIR/../bin" ]; then
    while IFS= read -r -d '' object; do queue+=("$object"); done \
        < <(find "$DEPLOY_DIR/../bin" -type f -print0)
fi

copied=0
for ((index = 0; index < ${#queue[@]}; index++)); do
    object="${queue[index]}"
    [ -z "${visited[$object]:-}" ] || continue
    visited["$object"]=1
    while read -r name; do
        [ -n "$name" ] || continue
        if source="$(find_source "$name")"; then
            if [ -n "${used[$name]:-}" ]; then continue; fi
            copy_with_links "$source"
            copied=$((copied + 1))
            queue+=("$source")
        elif [ -e "$DEPLOY_DIR/$name" ]; then
            while read -r file; do
                if [ -n "${managed[$file]:-}" ]; then used["$file"]=1; fi
            done < <(chain_files "$DEPLOY_DIR/$name")
            queue+=("$DEPLOY_DIR/$name")
        fi
    done < <(needed_of "$object")
done

for name in "${!managed[@]}"; do
    if [ -z "${used[$name]:-}" ]; then rm -f -- "$DEPLOY_DIR/$name"; fi
done
: > "$RECORD"
for name in "${!used[@]}"; do printf '%s\n' "$name" >> "$RECORD"; done
echo "deploy_linux_plugin_deps: copied $copied librar$([ "$copied" = 1 ] && echo y || echo ies)"
