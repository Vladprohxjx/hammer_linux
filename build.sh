#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ACTION="${1:-launcher}"

usage() {
    cat <<'EOF'
Usage: ./build.sh [launcher]

Build the native Linux Hammer launcher.
The full Hammer editor is not yet Linux-buildable: its current UI depends on
Microsoft MFC and Windows-specific APIs.

Required:
  SOURCE_SDK_ROOT   Source SDK checkout or its src/ directory, containing public/

Optional:
  SOURCE_SDK_LIBDIR Directory containing Source SDK Linux shared libraries
  BUILD_DIR         Build directory (default: ./build-linux)
  BUILD_TYPE        CMake build type (default: Release)
  JOBS              Parallel build jobs

Example:
  SOURCE_SDK_ROOT=/path/to/source-sdk-2013/src \
  SOURCE_SDK_LIBDIR=/path/to/source-sdk-2013/src/lib/linux64 \
  ./build.sh
EOF
}

case "$ACTION" in
  -h|--help|help) usage; exit 0 ;;
  launcher) ;;
  hammer)
    echo "Hammer editor Linux port is not ready: MFC/Win32 UI dependencies must be replaced." >&2
    echo "Build the launcher instead: ./build.sh launcher" >&2
    exit 2
    ;;
  *) usage >&2; exit 2 ;;
esac

command -v cmake >/dev/null || { echo "Error: cmake is required." >&2; exit 1; }
command -v "${CXX:-c++}" >/dev/null || { echo "Error: C++ compiler is required." >&2; exit 1; }

SDK_ROOT="${SOURCE_SDK_ROOT:-}"
if [[ -n "$SDK_ROOT" && -d "$SDK_ROOT/src/public" && ! -d "$SDK_ROOT/public" ]]; then
  SDK_ROOT="$SDK_ROOT/src"
fi

if [[ -z "$SDK_ROOT" ]]; then
  for candidate in "$ROOT_DIR/../source-sdk-2013/src" "$ROOT_DIR/../source-sdk-2013" "$ROOT_DIR/../src"; do
    if [[ -f "$candidate/public/appframework/AppFramework.h" ]]; then SDK_ROOT="$candidate"; break; fi
  done
fi

if [[ -z "$SDK_ROOT" || ! -f "$SDK_ROOT/public/appframework/AppFramework.h" ]]; then
  echo "Error: Source SDK headers not found. Set SOURCE_SDK_ROOT." >&2
  exit 1
fi

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-linux}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"

args=(-S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DSOURCE_SDK_ROOT="$SDK_ROOT")
if [[ -n "${SOURCE_SDK_LIBDIR:-}" ]]; then args+=("-DSOURCE_SDK_LIBDIR=$SOURCE_SDK_LIBDIR"); fi

cmake "${args[@]}"
cmake --build "$BUILD_DIR" --target hammer_launcher --parallel "$JOBS"
echo "Build complete."
