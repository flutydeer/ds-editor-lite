#!/usr/bin/env bash
#
# Copies the shared libraries that a deployed plugin tree depends on into the directory that
# contains the tree.
#
#     deploy_linux_plugin_deps.sh <deploy-lib-dir> <source-lib-dir>...
#
# The source directories are searched in the given order, and the first directory that contains a
# library supplies it. cmake/LiteBuildApi.cmake passes the library directory of the configuration
# being built first (debug/lib for a Debug build, selected per configuration under a multi-config
# generator) and the release lib directory after it, so this script makes no selection itself.
#
# vcpkg has no applocal deployment on Linux, and applocal deployment covers only the libraries
# that the *executable* links. A plugin is loaded dynamically at run time and links libraries that
# the executable does not link, for example cpp-pinyin. No other step copies those libraries, and
# loading the plugin would fail with an error about a missing dependency instead of an error about
# the plugin.
#
# The library set is computed as a transitive closure instead of a fixed list: every NEEDED entry
# of every staged file is resolved against the source directories, and each newly copied library
# is examined in turn. A hand-maintained list would become outdated as soon as a plugin gains a
# dependency.
#
# NEEDED entries are read with objdump instead of being resolved with ldd. objdump reports the
# dependencies that a file declares regardless of whether the loader can currently find them;
# ldd cannot report them correctly after the RPATHs are rewritten.
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

# Prints the path of the library in the first source directory that contains it, or prints
# nothing and returns 1. Only the plain name is searched for: a NEEDED entry is a soname, and the
# file with that name is the file to copy.
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

# Copies one library and the symlink chain that leads to it, because a soname is usually a
# symlink to a versioned file and the loader opens the file by the soname.
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

# Record of the source of each deployed library, one line per library: the library name, then the
# modification time and size of its source.
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
            # A deployed library is copied again only if its source has changed, as happens when a
            # dependency is reinstalled. Neither the deployed contents nor the deployed modification
            # time detects that change, because the subsequent RPATH step rewrites every deployed
            # copy on every build. Each copy is therefore recorded by the modification time and size
            # of its source, and a differing record indicates a different source. Skipping every
            # existing file instead would retain the first copy permanently, and a plugin would then
            # continue to load an outdated library.
            signature="$(stat -c '%Y %s' "$(readlink -f "$source")")"
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
    # Every dependency resolves within a number of rounds equal to the depth of the dependency
    # graph. This bound only terminates the loop on a cycle of broken symlinks.
    if [ "$round" -ge 16 ]; then
        echo "deploy_linux_plugin_deps: stopped after $round rounds" >&2
        break
    fi
done

echo "deploy_linux_plugin_deps: copied $copied librar$([ "$copied" = 1 ] && echo y || echo ies)"
