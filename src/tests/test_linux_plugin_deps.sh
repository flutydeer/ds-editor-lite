#!/usr/bin/env bash
set -euo pipefail
deploy_script="$(realpath "$1")"
parent="$(realpath "$2")"
root="$(mktemp -d "$parent/plugin-deps.XXXXXX")"
case "$root" in "$parent"/plugin-deps.*) ;; *) exit 1 ;; esac
trap 'rm -rf -- "$root"' EXIT
mkdir -p "$root/tools" "$root/source" "$root/app/lib/plugins" "$root/app/bin"

# Represent ELF NEEDED entries as text to isolate dependency traversal from the toolchain.
cat > "$root/tools/objdump" <<'EOF'
#!/usr/bin/env bash
cat "$2"
EOF
chmod +x "$root/tools/objdump"
export PATH="$root/tools:$PATH"
source_dir="$root/source"
lib="$root/app/lib"
run_deploy() { bash "$deploy_script" "$lib" "$source_dir"; }

printf 'NEEDED libold.so.1\n' > "$lib/plugins/plugin.so"
printf 'NEEDED libcycle.so.1\n' > "$source_dir/libold.so.1.0"
ln -s libold.so.1.0 "$source_dir/libold.so.1"
printf 'NEEDED libold.so.1\n' > "$source_dir/libcycle.so.1"
printf 'NEEDED libshared.so.1\n' > "$root/app/bin/editor"
printf 'shared-v1\n' > "$source_dir/libshared.so.1"
printf 'unmanaged\n' > "$lib/libunmanaged.so"
run_deploy
test -L "$lib/libold.so.1"
test -f "$lib/libold.so.1.0"
test -f "$lib/libcycle.so.1"
test -f "$lib/libshared.so.1"

# Accept records written by the earlier digest-based deployment script.
printf 'libold.so.1 old-digest\nlibcycle.so.1 old-digest\nlibshared.so.1 old-digest\n' > "$lib/.plugin-deps"
printf 'NEEDED libnew.so.2\n' > "$lib/plugins/plugin.so"
printf 'NEEDED libshared.so.1\n' > "$source_dir/libnew.so.2.0"
ln -s libnew.so.2.0 "$source_dir/libnew.so.2"
printf 'shared-v2\n' > "$source_dir/libshared.so.1"
run_deploy
test ! -e "$lib/libold.so.1"
test ! -e "$lib/libold.so.1.0"
test ! -e "$lib/libcycle.so.1"
test -L "$lib/libnew.so.2"
cmp "$source_dir/libshared.so.1" "$lib/libshared.so.1"
test -f "$lib/libunmanaged.so"

# Removing a plugin keeps dependencies still needed by the executable.
rm "$lib/plugins/plugin.so"
run_deploy
test ! -e "$lib/libnew.so.2"
test ! -e "$lib/libnew.so.2.0"
test -f "$lib/libshared.so.1"
test -f "$lib/libunmanaged.so"
run_deploy
test -f "$lib/libshared.so.1"
rm "$source_dir/libshared.so.1"
run_deploy
test -f "$lib/libshared.so.1"
echo 'Linux plugin deployment regression passed'
