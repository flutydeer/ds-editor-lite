#!/usr/bin/env bash
#
# Copies the shared libraries a deployed plugin tree needs into the directory beside it.
#
#     deploy_linux_plugin_deps.sh <deploy-lib-dir> <source-lib-dir>...
#
# vcpkg has no applocal deployment on Linux, and what it would deploy anyway is what the
# *executable* links. A plugin is loaded by name at runtime and links things the executable never
# does -- cpp-pinyin, for one -- so nothing copies those, and the plugin then fails to load with a
# message about a library rather than about itself.
#
# The set is closed rather than listed: every NEEDED entry of everything already staged is
# resolved against the source directories, and anything newly copied is examined in turn. Listing
# it by hand would be a list that goes stale the first time a plugin gains a dependency.
#
# NEEDED is read with objdump rather than resolved with ldd, so this says what a file asks for
# regardless of whether the loader can currently find it -- which is the question here, and which
# ldd stops being able to answer the moment RPATHs are rewritten.
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

# A library in a source directory, or nothing. Only the plain name is looked for: a NEEDED entry
# is a soname, and the file carrying it is what has to be copied.
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

# Copies one library and the symlink chain that leads to it, since a soname is usually a link to a
# versioned file and the loader follows the name it was given.
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

copied=0
round=0
while :; do
    round=$((round + 1))
    added=0
    while IFS= read -r -d '' object; do
        while read -r name; do
            [ -n "$name" ] || continue
            [ -e "$DEPLOY_DIR/$name" ] && continue
            source="$(find_source "$name")" || continue
            copy_with_links "$source"
            added=$((added + 1))
            copied=$((copied + 1))
        done < <(needed_of "$object")
    done < <(find "$DEPLOY_DIR" -type f \( -name '*.so' -o -name '*.so.*' \) -print0)

    [ "$added" -eq 0 ] && break
    # Every dependency resolves in at most as many rounds as the graph is deep; this bound only
    # stops a cycle of broken symlinks from spinning.
    if [ "$round" -ge 16 ]; then
        echo "deploy_linux_plugin_deps: giving up after $round rounds" >&2
        break
    fi
done

echo "deploy_linux_plugin_deps: copied $copied librar$([ "$copied" = 1 ] && echo y || echo ies)"
