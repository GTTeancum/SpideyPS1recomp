#!/usr/bin/env python3
"""Retired Repair05 cache tool. Original SFD playback replaces this approach."""
import sys
MESSAGE = "OSMV conversion/installation is retired. Use original unchanged .SFD files in mods/movies/dreamcast. Optional Dreamcast-disc installation is deferred to Codex."
if __name__ == '__main__':
    print(MESSAGE, file=sys.stderr)
    raise SystemExit(2)
