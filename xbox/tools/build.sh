#!/usr/bin/env bash
# Build the Xbox XBE with nxdk from Windows (MSYS2 bash + the installed LLVM).
#   C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh [make args]
# Run from the repository root. Output: xbox/build/release/default.xbe
set -euo pipefail

NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
LLVM_BIN="${LLVM_BIN:-/c/Program Files/LLVM/bin}"

export NXDK_DIR
export PATH="${LLVM_BIN}:${NXDK_DIR}/bin:/usr/bin:/mingw64/bin:${PATH}"

cd "$(dirname "$0")/.."
exec make "$@"
