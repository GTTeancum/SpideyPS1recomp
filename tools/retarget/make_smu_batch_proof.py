#!/usr/bin/env python3
"""Render actual converted batch assets and native swing-line capture; NOT gameplay."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from proof_render import load_asset,triangles,render,font
from animation_bank import read
from web_attachment import read_capture
from batch_smu import save_json

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ['batch','bank','out']:ap.add_argument('--'+n,type=Path,required=True)
 a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True);state=json.loads((a.batch/'batch-progress.json').read_text());bank=read(a.bank)
 columns=5;w,h=310,560;rows=(len(state['results'])+columns-1)//columns;header=98;footer=72
 image=Image.new('RGB',(columns*w,header+rows*h+footer),(15,20,29));d=ImageDraw.Draw(image)
 d.text((20,13),'SMU BATCH 01 — preserved rigs, original proportions',font=font(26),fill=(237,242,250))
 d.text((20,53),'Actual native PSX assets + compiled retarget core. Same unit scale. OFFLINE DIAGNOSTIC — NOT GAMEPLAY.',font=font(17),fill=(175,196,219))
 records=[]
 for n,row in enumerate(state['results']):
  folder=a.batch/'suits'/row['id'];asset=folder/'actor.psx';parsed,rig=load_asset(asset);tris,bones,error=triangles(parsed,rig)
  im=render(tris,Image.open(folder/'textures/diffuse.png'),width=w,height=h-44,yaw=-12,scale=.118)
  label=json.loads((folder/'suit.json').read_text())['name'];panel=Image.new('RGB',(w,h),(15,20,29));panel.paste(im,(0,44));pd=ImageDraw.Draw(panel);pd.text((12,8),label,font=font(18),fill=(227,238,248));pd.text((12,h-25),str(row['sourceTriangles'])+' triangles | '+str(rig.bones)+' rig nodes',font=font(13),fill=(157,182,205))
  image.paste(panel,((n%columns)*w,header+(n//columns)*h));panel.save(a.out/(row['key']+'.png'))
  records.append(dict(key=row['key'],assetSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),kind='calibrated neutral pose, original source fist policy',maxS16PositionComponentError=error))
 d.text((20,image.height-57),'8 newly converted suits + Spider-Ham and 2099 regression controls. No FBX rigs or skin weights replaced.',font=font(19),fill=(160,230,196))
 d.text((20,image.height-27),'Small-batch checkpoint: 10 / 233 ready-catalogue costumes verified. Full game / C# runtime not run.',font=font(16),fill=(168,187,209))
 image.save(a.out/'SMU-Batch01-Proof.png')
 # A separate actual swing pose, with coordinates from the executed native web loop.
 key='gwenom';row=next(r for r in state['results'] if r['key']==key);folder=a.batch/'suits'/row['id'];parsed,rig=load_asset(folder/'actor.psx')
 rec=next(r for r in read_capture(a.batch/'tests/web'/f'{key}-swing.bin') if r['clip']==275 and r['frame']==25)
 tris,_,_=triangles(parsed,rig,bank[275][25]);yaw=-50;scale=.15;w,h=800,950;theta=np.deg2rad(yaw);R=np.array([[np.cos(theta),0,np.sin(theta)],[0,1,0],[-np.sin(theta),0,np.cos(theta)]])
 pts=np.concatenate([t['p'] for t in tris])@R.T;center=[(pts[:,0].min()+pts[:,0].max())/2,-450];offset=np.array([w/2,h/2])-np.array(center)*scale
 im=render(tris,Image.open(folder/'textures/diffuse.png'),w,h,yaw,scale,center,ground=False);dd=ImageDraw.Draw(im);segments=rec['after'];line=np.vstack([segments[0,:3],segments[:,3:]])/256@R.T;xy=line[:,:2]*scale+offset
 dd.line([tuple(v) for v in xy],fill=(240,246,252),width=3);x,y=xy[-1];dd.ellipse((x-6,y-6,x+6,y+6),outline=(93,231,170),width=2)
 dd.rectangle((0,0,w,88),fill=(15,20,29));dd.text((20,10),'GWENOM — original swing clip 275 / frame 25',font=font(24),fill=(237,242,250));dd.text((20,49),'Native captured web endpoint follows the retargeted wrist.',font=font(18),fill=(162,227,191))
 dd.text((20,h-36),'OFFLINE NATIVE-CODE DIAGNOSTIC — NOT GAMEPLAY',font=font(18),fill=(180,198,218));im.save(a.out/'SMU-Gwenom-Swing-Proof.png')
 save_json(a.out/'PROOF-PROVENANCE.json',dict(scope='Offline native-asset/core images; no running game, no native GPU projection/occlusion',neutralCamera=dict(yaw=-12,pixelsPerModelUnit=.118),records=records,swing=dict(key=key,clip=275,frame=25,captureSha256=hashlib.sha256((a.batch/'tests/web'/f'{key}-swing.bin').read_bytes()).hexdigest(),endpointErrorQ12=0,anchor='Controlled native-oracle fixture, not a saved gameplay scene')))
if __name__=='__main__':main()
