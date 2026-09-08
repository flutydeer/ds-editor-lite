#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
qt_root="${QT_ROOT_DIR:-${QT_DIR:-}}"
if [[ -z "$qt_root" || ! -d "$qt_root/lib/cmake/Qt6" ]]; then
    echo 'Set QT_ROOT_DIR to a Qt installation with the project modules.' >&2
    exit 2
fi

export QT_DIR="$qt_root"
export Qt6_DIR="$qt_root/lib/cmake/Qt6"
export Qt6GuiTools_DIR="$qt_root/lib/cmake/Qt6GuiTools"
export CMAKE_PREFIX_PATH="$qt_root"
export VCPKG_KEEP_ENV_VARS='QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH'

vcpkg_commit=abb6dda5cc32914d2e64d7d72b974dc301d1fc8a
if [[ ! -d "$repo_root/vcpkg/.git" ]]; then
    git init "$repo_root/vcpkg"
    git -C "$repo_root/vcpkg" remote add origin https://github.com/microsoft/vcpkg.git
    git -C "$repo_root/vcpkg" fetch --depth 1 origin "$vcpkg_commit"
    git -C "$repo_root/vcpkg" checkout --detach FETCH_HEAD
elif [[ "$(git -C "$repo_root/vcpkg" rev-parse HEAD)" != "$vcpkg_commit" ]]; then
    echo "Existing vcpkg checkout must be at $vcpkg_commit; it has not been modified." >&2
    exit 2
fi

"$repo_root/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
"$repo_root/vcpkg/vcpkg" install \
    --x-manifest-root="$repo_root/scripts/vcpkg-manifest" \
    --x-install-root="$repo_root/vcpkg/installed" \
    --triplet=x64-linux --clean-buildtrees-after-build
