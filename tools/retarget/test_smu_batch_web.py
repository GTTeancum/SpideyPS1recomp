#!/usr/bin/env python3
"""Run the original native socket/line oracle against every suit in a saved batch.

This runs compiled native routines and retarget exports, NOT the managed game.
Saves per-suit captures and checkpoints; rejects mismatches immediately.
"""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
import numpy as np
from proof_render import load_asset
from animation_bank import read
from web_attachment import read_capture,pose,wrist
from batch_smu import save_json

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ['batch','bank','oracle','out']:ap.add_argument('--'+key,required=True,type=Path)
 a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True);journal=json.loads((a.batch/'batch-progress.json').read_text());bank=read(a.bank);records=[]
 if any(r['status']!='converted' for r in journal['results']):raise ValueError('Incomplete batch')
 for row in journal['results']:
  k=row['key'];d=a.batch/'suits'/row['id'];asset=d/'actor.psx';parsed,rig=load_asset(asset)
  blob=a.out/(k+'.rtg');blob.write_bytes(rig.blob);capture=a.out/(k+'-swing.bin')
  proc=subprocess.run([str(a.oracle),str(blob),str(a.bank),str(capture)],capture_output=True,text=True)
  (a.out/(k+'-oracle.log')).write_text(proc.stdout+proc.stderr)
  if proc.returncode:raise RuntimeError('Native web oracle failed for '+k+': '+proc.stderr)
  rows=read_capture(capture);assert len(rows)==115 and {r['clip'] for r in rows}=={275,280}
  gaps=[];maxerr=0
  for r in rows:
   p=pose(rig,bank[r['clip']][r['frame']]);target=wrist(rig,p,r['part']);after=r['after']
   # Independent source-bone mapping, not just a visual line drawn to a guessed hand.
   matches=[i for i in range(rig.bones) if struct.unpack_from('<i',rig.blob,rig.h[7]+i*196+4)[0]==r['part']]
   assert len(matches)==1
   q=p[matches[0],:,3].astype(np.float64)*256
   expected=np.where(q>=0,np.floor(q+.5),np.ceil(q-.5)).astype(np.int64)
   assert np.array_equal(expected,target) and np.array_equal(target,r['target']) and np.array_equal(after[-1,3:],target)
   assert np.array_equal(after[0,:3],r['before'][0,:3]) and np.array_equal(after[:-1,3:],after[1:,:3])
   for primary,alias in [(5,6),(10,11)]:assert np.array_equal(wrist(rig,p,primary),wrist(rig,p,alias))
   gaps.append((int(r['target'][1])-int(r['before'][-1,4]))/4096)
   maxerr=max(maxerr,int(np.max(abs(after[-1,3:].astype(np.int64)-target.astype(np.int64)))))
  records.append(dict(key=k,id=row['id'],status='PASS',swingFrames=115,nativeSegments=115*16,
      maxEndpointErrorQ12=maxerr,beforeVerticalGapWorldUnits=[min(gaps),max(gaps)],
      assetSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),captureSha256=hashlib.sha256(capture.read_bytes()).hexdigest()))
  save_json(a.out/'WEB_BATCH_RESULTS.json',dict(status='PASS-so-far' if len(records)<len(journal['results']) else 'PASS',
   scope='Original native socket/line + retarget core; controlled offline actor/anchor fixture, NOT gameplay',
   complete=len(records),requested=len(journal['results']),results=records))
  print('PASS '+k+': 115 native swing frames, exact wrist endpoints, unchanged anchor/actor/rope buffers and no segment seams',flush=True)
 return 0
if __name__=='__main__':raise SystemExit(main())
