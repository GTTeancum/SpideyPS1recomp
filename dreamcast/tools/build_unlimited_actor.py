#!/usr/bin/env python3
"""Default SMU build path: preserve the original rig in native RTG2 assets.

--legacy-template explicitly invokes the old destructive template fitter.
It is never used as an automatic fallback for unsupported source rigs.
"""
import argparse,runpy,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
if '--legacy-template' in sys.argv:
    sys.argv.remove('--legacy-template')
    print('WARNING: legacy template mode replaces source weights and does not preserve the FBX rig.',file=sys.stderr)
    runpy.run_path(str(Path(__file__).with_name('legacy_build_unlimited_actor.py')),run_name='__main__')
    raise SystemExit
sys.path.insert(0,str(ROOT/'tools'/'retarget'))
from convert import convert
from fbx_binary import Fbx

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True,help='FBX file or folder containing exactly one FBX')
    p.add_argument('--reference',type=Path,required=True,help='normal-proportioned 2099 FBX; common size reference, not a replacement rig')
    p.add_argument('--texture',type=Path,required=True,help='original RGB diffuse PNG/TGA')
    p.add_argument('--donor',type=Path,default=ROOT/'spiderman'/'assets'/'builtin'/'spidey.psx')
    p.add_argument('--output',type=Path,required=True,help='new native suit directory')
    p.add_argument('--id',required=True);p.add_argument('--name',required=True)
    a=p.parse_args();fbx=a.source
    if fbx.is_dir():
        found=list(fbx.glob('*.fbx'))
        if len(found)!=1:p.error('source folder must contain exactly one FBX')
        fbx=found[0]
    try:convert(fbx,a.reference,a.donor,a.texture,a.output,a.id,a.name)
    except (ValueError,OSError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
