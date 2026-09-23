"""Stage a small normal conversion batch while preserving existing packaging."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
from types import SimpleNamespace

import numpy as np
from PIL import Image

repo=Path('C:/Programming/GitHub/OpenSpideyPS1')
sys.path.insert(0,str(repo/'tools/retarget'))
from batch_smu import run, save_json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--keys',nargs='+',required=True)
args=parser.parse_args()
out=args.out.resolve()
status=run(SimpleNamespace(samples=Path('C:/Programming/SMU-Costumes'),
    donor=repo/'spiderman/extracted/wad/spidey.psx',keys=args.keys,out=out,batch=out.name))
if status:
    raise SystemExit(status)
journal=json.loads((out/'batch-progress.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
preserved=[]
for row in journal['results']:
    old=repo/'spiderman/port/mods/suits'/row['id']
    new=out/'suits'/row['id']
    previous=json.loads((old/'suit.json').read_text())
    converted=json.loads((new/'suit.json').read_text())
    assert {k:v for k,v in previous.items() if k not in ('name','comments')}=={
        k:v for k,v in converted.items() if k not in ('name','comments')}, row['id']+' manifest semantics changed'
    records=[]
    for relative in ['suit.json',*previous['textures'].values()]:
        a,b=old/relative,new/relative
        if relative!='suit.json':
            with Image.open(a) as ai, Image.open(b) as bi:
                assert np.array_equal(np.asarray(ai.convert('RGBA')),np.asarray(bi.convert('RGBA'))), row['id']+' texture pixels changed'
        if sha(a)!=sha(b):
            records.append(dict(file=relative,generatedSha256=sha(b),retainedSha256=sha(a)))
            shutil.copy2(a,b)
    for relative in row['outputs']:
        row['outputs'][relative]=sha(new/relative)
    preserved.append(dict(suit=row['id'],beforeActorSha256=sha(old/'actor.psx'),
        manifestSemanticsAndDecodedPixelsIdentical=True,retainedFiles=records))
save_json(out/'batch-progress.json',journal)
save_json(out/'packaging-preservation.json',preserved)
print('Original display metadata and texture bytes preserved for',len(preserved),'suits.')
