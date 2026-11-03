#!/usr/bin/env bash
# Compile one game's generated C with nxdk's compiler (no link): the check that
# RecompOne's C output is valid C for the Xbox target.
#   C:/msys64/usr/bin/bash.exe --noprofile --norc xbox/tools/compile_generated.sh spiderman
# Objects: xbox/build/generated/<game>/; errors: xbox/build/generated/<game>/errors.log
set -uo pipefail

NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
LLVM_BIN="${LLVM_BIN:-/c/Program Files/LLVM/bin}"
export NXDK_DIR
export PATH="${LLVM_BIN}:${NXDK_DIR}/bin:/usr/bin:/mingw64/bin:${PATH}"

cd "$(dirname "$0")/.."
game="$1"
src="generated/$game"
out="build/generated/$game"
mkdir -p "$out"
: > "$out/errors.log"

ls "$src"/*.c | xargs -P 8 -I{} sh -c '
    f="{}"; o="'"$out"'/$(basename "${f%.c}").obj"
    if ! nxdk-cc -c -O2 -Wall -Wno-unused-label -Wno-unused-variable -I runtime -I "'"$src"'" "$f" -o "$o" 2>"$o.err"; then
        echo "FAILED $f" >> "'"$out"'/errors.log"; cat "$o.err" >> "'"$out"'/errors.log"
    elif [ -s "$o.err" ]; then cat "$o.err" >> "'"$out"'/errors.log"; fi
    rm -f "$o.err"'

files=$(ls "$src"/*.c | wc -l)
objs=$(ls "$out"/*.obj 2>/dev/null | wc -l)
failed=$(grep -c '^FAILED' "$out/errors.log")
warnings=$(grep -c 'warning:' "$out/errors.log")
echo "$game: $objs of $files compiled, $failed failed, $warnings warnings"
[ "$failed" -eq 0 ]
