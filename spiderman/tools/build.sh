#!/usr/bin/env bash
# Build the checked-in Spider-Man generated code; never invoke the old X-Men template.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
exec "$root/BUILD-LINUX.sh" --game 1 "$@"
