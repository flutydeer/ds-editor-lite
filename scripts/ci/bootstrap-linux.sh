#!/usr/bin/env bash
set -euo pipefail

exec python3 "$(dirname -- "${BASH_SOURCE[0]}")/bootstrap-vcpkg.py" --triplet x64-linux
