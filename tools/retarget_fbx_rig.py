#!/usr/bin/env python3
"""Replacement entry point. The unsafe legacy scale-profile patch is retired."""
from pathlib import Path
import runpy,sys
if any(x in sys.argv for x in ('--target','--profile','--model')):
    raise SystemExit('The old scale-profile/object-table patch is unsafe and has been retired. Use tools/retarget/convert.py --help for full native conversion with preserved rigging. Existing converted actors cannot acquire a missing FBX rig from scale ratios.')
sys.path.insert(0,str(Path(__file__).resolve().parent/'retarget'))
runpy.run_path(str(Path(__file__).resolve().parent/'retarget'/'convert.py'),run_name='__main__')
