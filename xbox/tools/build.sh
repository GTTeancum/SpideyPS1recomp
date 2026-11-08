#!/usr/bin/env bash
# Build the Xbox XBE with nxdk from Windows (MSYS2 bash + the installed LLVM).
#   C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh [make args]
# Run from the repository root. Output: xbox/build/release/default.xbe
set -euo pipefail

NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
# The Windows Python (with Pillow) for the post-build tools, found before PATH puts
# MSYS2's own first.
PYTHON="${PYTHON:-$(command -v python.exe || command -v python)}"
LLVM_BIN="${LLVM_BIN:-/c/Program Files/LLVM/bin}"

export NXDK_DIR
export PATH="${LLVM_BIN}:${NXDK_DIR}/bin:/usr/bin:/mingw64/bin:${PATH}"

cd "$(dirname "$0")/.."
make "$@"
# The title's own ID in the certificate (cxbe writes the one every nxdk program shares)
# and the dashboard images from the game icon, beside the XBE.
"$PYTHON" -I tools/patch_xbe.py build/release/default.xbe
"$PYTHON" -I tools/make_xbox_images.py ../spiderman/port/OpenSpidey.png build/release
