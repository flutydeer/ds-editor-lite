#!/usr/bin/env bash
set -euo pipefail

NORMALIZE=false
PATTERNS=()
EXCEPTIONS=()
VERBOSE=1  # default verbosity
ARGS=()

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --normalize)
            NORMALIZE=true
            ;;
        --pattern=*)
            PATTERNS+=("${arg#--pattern=}")
            ;;
        --except=*)
            IFS=',' read -ra patterns <<< "${arg#--except=}"
            for p in "${patterns[@]}"; do
                EXCEPTIONS+=("$p")
            done
            ;;
        --verbose=*)
            VERBOSE="${arg#--verbose=}"
            ;;
        *)
            ARGS+=("$arg")
            ;;
    esac
done

# Expect exactly two non-flag arguments
if [[ ${#ARGS[@]} -ne 2 ]]; then
    echo "Usage: $0 [--normalize] [--pattern=PATTERN ...] [--except=PATTERN1,PATTERN2,...] [--verbose=N] <target_dir> <rpath_dir>"
    exit 1
fi

TARGET_DIR="${ARGS[0]}"
RPATH_DIR="${ARGS[1]}"

# Default pattern if none specified
if [[ ${#PATTERNS[@]} -eq 0 ]]; then
    PATTERNS=('lib*.so')
fi

# Normalize RPATH_DIR if requested
if $NORMALIZE; then
    RPATH_DIR="$(realpath -s "$RPATH_DIR")"
fi

# Check directories exist
if [[ ! -d "$TARGET_DIR" ]]; then
    echo "Target directory not found: $TARGET_DIR" >&2
    exit 1
fi

if [[ ! -d "$RPATH_DIR" ]]; then
    echo "Rpath directory not found: $RPATH_DIR" >&2
    exit 1
fi

# Logging function
log() {
    local level="$1"
    shift
    if (( VERBOSE >= level )); then
        echo "$@"
    fi
}

# Function to check if a file matches any exception pattern
is_exception() {
    local file="$1"
    local base
    base="$(basename "$file")"
    for pattern in "${EXCEPTIONS[@]}"; do
        if [[ "$base" == $pattern ]]; then
            return 0
        fi
    done
    return 1
}

# Apply patchelf to each matching file
for pat in "${PATTERNS[@]}"; do
    find "$TARGET_DIR" -type f -name "$pat" -print0 | while IFS= read -r -d '' f; do
        if is_exception "$f"; then
            log 2 "Skipping exception: $f"
            continue
        fi
        log 1 "Adding rpath of $f to $RPATH_DIR"
        log 2 "Command: patchelf --add-rpath \"$RPATH_DIR\" \"$f\""
        patchelf --add-rpath "$RPATH_DIR" "$f"
    done
done
