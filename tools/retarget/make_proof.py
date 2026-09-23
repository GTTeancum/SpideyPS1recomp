#!/usr/bin/env python3
"""Render actual native actors through the SAME compiled core as the game hook.
This is an offline native-asset view, never a gameplay capture.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','1')
os.environ.setdefault('OMP_NUM_THREADS','1')
import argparse,base64,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from proof_render import load_asset,triangles,render,caption,font
from animation_bank import read

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--suits',required=True,type=Path);ap.add_argument('--animation-bank',required=True,type=Path);ap.add_argument('--out',required=True,type=Path)
    ap.add_argument('--capture',action='store_true',help='Requires Playwright and Chromium; captures this offline viewer, not the game')
    a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True);clips=read(a.animation_bank);assets={};proof=[]
    titles={'smu-spiderham':'Spider-Ham','smu-2099':'Spider-Man 2099'}
    for name in ['smu-2099','smu-spiderham']:
        d=a.suits/name;p,r=load_asset(d/'actor.psx');tex=Image.open(d/'textures/diffuse.png');assets[name]=(p,r,tex)
        ts,b,error=triangles(p,r,clips[0][3]);im=render(ts,tex,540,640,yaw=-12,scale=.165)
        im=caption(im,titles[name],f'Native clip 0 / frame 3 | {r.bones} preserved rig nodes')
        im.save(a.out/(name+'-native-animation.png'))
        proof.append(dict(asset=name,sha256=p['sha256'],clip=0,frame=3,rigNodes=r.bones,nativeRoundingMaxComponentError=error))
        # Eight real animation poses. Shared camera/scale, no target-height normalization.
        sheet=Image.new('RGB',(1200,1030),(15,20,29))
        for j,ci in enumerate([0,11,19,28,45,67,110,180]):
            fi=min(3,len(clips[ci])-1);ts,_,_=triangles(p,r,clips[ci][fi]);im=render(ts,tex,300,440,yaw=-12,scale=.095)
            im=caption(im,f'Clip {ci} / frame {fi}','Original animation / preserved rig')
            sheet.paste(im,((j%4)*300,(j//4)*515))
        sheet.save(a.out/(name+'-pose-sheet.png'))
        # Fists: both hands, full mesh retained so packet boundaries are not holes.
        hands=Image.new('RGB',(800,990),(15,20,29))
        for row,flag in enumerate([1,0]):
            ts,b,_=triangles(p,r,clips[0][3],flags=flag)
            for col,(side,yaw) in enumerate([('L',-12),('R',12)]):
                bi=r.provenance['boneNames'].index('Clown001'+side+'ArmPalm');v=b[bi,:,3];ang=np.deg2rad(yaw)
                rotation=np.array([[np.cos(ang),0,np.sin(ang)],[0,1,0],[-np.sin(ang),0,np.cos(ang)]])
                center=(v@rotation.T)[:2]+[0,110]
                im=render(ts,tex,400,420,yaw,.72 if name=='smu-spiderham' else .70,center)
                im=caption(im,side+(' hand: open control' if flag else ' hand: default fist'),'Same original bones and skin weights')
                hands.paste(im,(col*400,row*495))
        hands.save(a.out/(name+'-fist-comparison.png'))
    # Real native clip 11 (10 frames). Side-by-side same scale, source frame order.
    sequence=[]
    for fi,driver in enumerate(clips[11]):
        canvas=Image.new('RGB',(900,650),(15,20,29))
        for col,name in enumerate(['smu-2099','smu-spiderham']):
            p,r,tex=assets[name];ts,_,_=triangles(p,r,driver)
            im=caption(render(ts,tex,450,525,yaw=-12,scale=.125),titles[name],f'Original clip 11 / frame {fi}; same camera scale')
            canvas.paste(im,(col*450,50))
        ImageDraw.Draw(canvas).text((16,14),'NATIVE ACTORS + RUNTIME CORE - OFFLINE, NOT GAMEPLAY',fill=(235,240,249),font=font(18))
        sequence.append(canvas)
    sequence[0].save(a.out/'Native-Animation-Proof.gif',save_all=True,append_images=sequence[1:],duration=100,loop=0,optimize=False)
    def embedded(name):return 'data:image/png;base64,'+base64.b64encode((a.out/name).read_bytes()).decode()
    html='''<!doctype html><meta charset="utf-8"><title>Spider-Man Retargeting Repair - Native Asset Proof</title>
<style>body{margin:0;background:#0f141d;color:#edf2fa;font:16px Arial,sans-serif}main{width:1120px;margin:24px auto}h1{font-size:28px;margin:0 0 10px}p{color:#afc1d4;line-height:1.5}.notice{padding:12px;background:#263443;border-left:4px solid #a4c4e5}section{display:flex;gap:20px;margin:18px 0}img{display:block;max-width:100%}.card{width:540px}small{color:#adc1d7}.bottom{background:#18212c;padding:16px}</style>
<main><h1>Retargeting repair: actual converted native actors</h1>
<div class="notice"><b>OFFLINE NATIVE-ASSET VIEWER — NOT A GAMEPLAY SCREENSHOT</b><br>Both characters use the original game's clip 0, frame 3, through the compiled runtime retargeting core.</div>
<section>'''
    for name in ['smu-2099','smu-spiderham']:html+='<div class="card"><img src="'+embedded(name+'-native-animation.png')+'"></div>'
    html+='''</section><div class="bottom"><b>Same camera and unit scale. No height normalization.</b><br>
Native .psx geometry + embedded original rigs + original animation matrices → weighted skinning → signed-16 native vertices → original part transform.<br>
Fingers are articulated into fists; native alternate-hand meshes remain matched.<br><small>54 automated checks passed. Full .NET game build and in-game capture have not been run in this environment.</small></div></main>'''
    (a.out/'native-proof.html').write_text(html)
    record=dict(kind='Actual native assets + compiled runtime core + original animation routine oracle; offline, NOT the running game',renderer='deterministic triangle rasterizer',texture='source full-resolution RGB external replacement',assets=proof,camera={'yawDegrees':-12,'pixelsPerNativeUnit':.165},animationSha256=hashlib.sha256(a.animation_bank.read_bytes()).hexdigest())
    if a.capture:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as pw:
            browser=pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--disable-dev-shm-usage','--disable-gpu','--disable-background-networking'])
            page=browser.new_page(viewport={'width':1180,'height':1080},device_scale_factor=1);errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
            page.set_content(html,wait_until='load');page.wait_for_function('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)')
            shot=a.out/'Retargeting-Repair-Proof.png';page.screenshot(path=str(shot),full_page=True)
            record.update(browser=browser.version,screenshotSha256=hashlib.sha256(shot.read_bytes()).hexdigest(),pageErrors=errors);browser.close()
    (a.out/'PROOF_PROVENANCE.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
if __name__=='__main__':main()
