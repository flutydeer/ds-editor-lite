#!/usr/bin/env bash
set -euo pipefail

# Script to fix Qt framework paths in dylibs for macOS deployment
# Replaces hardcoded paths like /opt/homebrew/*/QtCore.framework with @rpath

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <bundle_dir> [verbose]"
    echo "  bundle_dir: Path to .app bundle (e.g., /path/to/App.app)"
    echo "  verbose: Set to 1 for verbose output (optional)"
    exit 1
fi

BUNDLE_DIR="$1"
VERBOSE="${2:-0}"

if [[ ! -d "$BUNDLE_DIR" ]]; then
    echo "Error: Bundle directory not found: $BUNDLE_DIR" >&2
    exit 1
fi

# Extract .app name if full path is provided
if [[ "$BUNDLE_DIR" == *.app ]]; then
    APP_NAME=$(basename "$BUNDLE_DIR" .app)
    FRAMEWORKS_DIR="$BUNDLE_DIR/Contents/Frameworks"
else
    echo "Error: Bundle directory must be a .app bundle" >&2
    exit 1
fi

if [[ ! -d "$FRAMEWORKS_DIR" ]]; then
    echo "Warning: Frameworks directory not found: $FRAMEWORKS_DIR" >&2
    exit 0
fi

log() {
    if [[ "$VERBOSE" == "1" ]]; then
        echo "$@"
    fi
}

# Find all Qt frameworks in the bundle
QT_FRAMEWORKS=()
while IFS= read -r -d '' framework; do
    QT_FRAMEWORKS+=("$framework")
done < <(find "$FRAMEWORKS_DIR" -type d -name "Qt*.framework" -print0 2>/dev/null || true)

if [[ ${#QT_FRAMEWORKS[@]} -eq 0 ]]; then
    log "No Qt frameworks found in $FRAMEWORKS_DIR"
    exit 0
fi

# Build list of Qt framework names (for bash 3.x compatibility, we'll use a function instead of associative array)
QT_FRAMEWORK_NAMES=()
for framework in "${QT_FRAMEWORKS[@]}"; do
    framework_name=$(basename "$framework" .framework)
    # Framework binary is at FrameworkName.framework/Versions/A/FrameworkName
    framework_binary="$framework/Versions/A/$framework_name"
    if [[ -f "$framework_binary" ]]; then
        QT_FRAMEWORK_NAMES+=("$framework_name")
        log "Found Qt framework: $framework_name"
    fi
done

# Function to get @rpath for a framework name
get_framework_rpath() {
    local framework_name="$1"
    echo "@rpath/$framework_name.framework/Versions/A/$framework_name"
}

# Function to check if a framework exists in our bundle
has_framework() {
    local framework_name="$1"
    for name in "${QT_FRAMEWORK_NAMES[@]}"; do
        if [[ "$name" == "$framework_name" ]]; then
            return 0
        fi
    done
    return 1
}

# Function to fix a single dylib
fix_dylib() {
    local dylib="$1"
    local changed=false
    
    # Get all dependencies
    local deps
    deps=$(otool -L "$dylib" 2>/dev/null | grep -E "Qt.*\.framework" | awk '{print $1}' || true)
    
    if [[ -z "$deps" ]]; then
        return 0
    fi
    
    while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        
        # Check if this is a hardcoded path (not @rpath or @loader_path)
        if [[ "$dep" != @* ]]; then
            # Extract framework name (e.g., QtCore from /path/to/QtCore.framework/...)
            # Match patterns like: /opt/homebrew/*/QtCore.framework/Versions/A/QtCore
            # or /usr/local/Qt-6.x.x/lib/QtCore.framework/Versions/A/QtCore
            if [[ "$dep" =~ ([^/]+)\.framework ]]; then
                framework_name="${BASH_REMATCH[1]}"
                
                # Check if we have this framework in our bundle
                if has_framework "$framework_name"; then
                    new_path=$(get_framework_rpath "$framework_name")
                    log "Fixing $dylib: $dep -> $new_path"
                    if install_name_tool -change "$dep" "$new_path" "$dylib" 2>/dev/null; then
                        changed=true
                    else
                        echo "Warning: Failed to fix $dep in $dylib" >&2
                    fi
                fi
            fi
        fi
    done <<< "$deps"
    
    # Add @rpath to the dylib's rpath list if not present
    local rpaths
    rpaths=$(otool -l "$dylib" 2>/dev/null | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)
    
    if [[ "$rpaths" != *"@loader_path/../Frameworks"* ]] && [[ "$rpaths" != *"@executable_path/../Frameworks"* ]]; then
        log "Adding @loader_path/../Frameworks to $dylib"
        install_name_tool -add_rpath "@loader_path/../Frameworks" "$dylib" 2>/dev/null || {
            # If @loader_path doesn't work, try @executable_path
            install_name_tool -add_rpath "@executable_path/../Frameworks" "$dylib" 2>/dev/null || {
                echo "Warning: Failed to add rpath to $dylib" >&2
            }
        }
        changed=true
    fi
    
    if [[ "$changed" == "true" ]]; then
        log "Fixed dylib: $dylib"
    fi
}

# Find and fix all dylibs in the bundle
dylib_count=0
fixed_count=0

while IFS= read -r -d '' dylib; do
    dylib_count=$((dylib_count + 1))
    if fix_dylib "$dylib"; then
        fixed_count=$((fixed_count + 1))
    fi
done < <(find "$FRAMEWORKS_DIR" -type f -name "*.dylib" -print0 2>/dev/null || true)

# Also fix the main executable
executable="$BUNDLE_DIR/Contents/MacOS/$APP_NAME"
if [[ -f "$executable" ]]; then
    dylib_count=$((dylib_count + 1))
    if fix_dylib "$executable"; then
        fixed_count=$((fixed_count + 1))
    fi
fi

log "Processed $dylib_count files, fixed $fixed_count"

