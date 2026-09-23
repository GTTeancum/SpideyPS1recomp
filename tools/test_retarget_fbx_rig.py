#!/usr/bin/env python3
"""The former length-only smoke test is retired. Run the full native/core suite."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent/'retarget'/'test_retarget.py'),run_name='__main__')
