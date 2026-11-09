#!/usr/bin/env bash
# Build the Xbox XBE with nxdk from Windows (MSYS2 bash + the installed LLVM).
#   C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/build.sh [make args]
#   GAME=spiderman2 ... xbox/tools/build.sh [make args]
# Run from the repository root. Output: xbox/build/release/default.xbe (Spider-Man,
# the default), xbox/build/sm2/default.xbe (GAME=spiderman2).
set -euo pipefail

GAME="${GAME:-spiderman}"
case "$GAME" in
  spiderman) OUT=build/release ;;
  spiderman2) OUT=build/sm2 ;;
  *) echo "GAME must be spiderman or spiderman2" >&2; exit 2 ;;
esac

NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
# The Windows Python (with Pillow) for the post-build tools, found before PATH puts
# MSYS2's own first.
PYTHON="${PYTHON:-$(command -v python.exe || command -v python)}"
LLVM_BIN="${LLVM_BIN:-/c/Program Files/LLVM/bin}"

export NXDK_DIR
export PATH="${LLVM_BIN}:${NXDK_DIR}/bin:/usr/bin:/mingw64/bin:${PATH}"

cd "$(dirname "$0")/.."
make GAME="$GAME" "$@"
# The dashboard images from the game icon, beside the XBE; then the title's own ID and
# name in the certificate (cxbe writes the ID every nxdk program shares) and the images
# inserted into the XBE, where launchers look for the icon.
"$PYTHON" -I tools/make_xbox_images.py ../spiderman/port/OpenSpidey.png "$OUT"
"$PYTHON" -I tools/patch_xbe.py "$OUT/default.xbe"
