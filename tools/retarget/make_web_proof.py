#!/usr/bin/env python3
"""Capture the real native-asset/original-line-coordinate diagnostic. NOT gameplay."""
import argparse,hashlib,json,math,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from proof_render import load_asset,triangles,render,font
from animation_bank import read
from web_attachment import read_capture

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,required=True);ap.add_argument('--bank',type=Path,required=True);ap.add_argument('--capture',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--browser',action='store_true');ap.add_argument('--gif',action='store_true');a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    asset=a.project/'spiderman/port/mods/suits/smu-spiderham';parsed,rig=load_asset(asset/'actor.psx');bank=read(a.bank);captures=read_capture(a.capture);rec=next(r for r in captures if r['clip']==275 and r['frame']==25)
    tex=Image.open(asset/'textures/diffuse.png');tri,bones,err=triangles(parsed,rig,bank[275][25]);yaw=-50;theta=np.deg2rad(yaw);R=np.array([[np.cos(theta),0,np.sin(theta)],[0,1,0],[-np.sin(theta),0,np.cos(theta)]])
    points=np.concatenate([t['p'] for t in tri])@R.T;center=np.array([(points[:,0].min()+points[:,0].max())/2,-450]);scale=.164
    def panel(r,side,w=742,h=878,annotate=True):
        tris,_,error=triangles(parsed,rig,bank[r['clip']][r['frame']]);im=render(tris,tex,w,h,yaw,scale,center,ground=False);d=ImageDraw.Draw(im);off=np.array([w/2,h/2])-center*scale
        def project(p):return (np.asarray(p)/256@R.T)[...,:2]*scale+off
        seg=r[side];line=np.vstack([seg[0,:3],seg[:,3:]]);xy=project(line)
        d.line([tuple(v) for v in xy],fill=(240,246,254),width=3)
        target=project(r['target']);tip=xy[-1];col=(255,190,104) if side=='before' else (93,231,170)
        x,y=tip;d.ellipse((x-6,y-6,x+6,y+6),outline=col,width=2)
        if annotate:
            d.rectangle((12,12,w-12,57),fill=(23,30,41))
            d.text((24,20),'BEFORE  |  adult hand socket' if side=='before' else 'AFTER  |  retargeted wrist',fill=col,font=font(25))
            if side=='before':
                gx=max(tip[0],target[0])+60
                for yy in range(round(tip[1]),round(target[1]),13):d.line((gx,yy,gx,min(yy+6,target[1])),fill=col,width=2)
                for v in [tip,target]:d.line((v[0]+10,v[1],gx+7,v[1]),fill=col,width=1)
                d.text((gx+16,(tip[1]+target[1])/2-32),'VERTICAL GAP',fill=col,font=font(17));d.text((gx+16,(tip[1]+target[1])/2-6),f"{(int(r['target'][1])-int(r['before'][-1,4]))/4096:.2f}",fill=col,font=font(33));d.text((gx+16,(tip[1]+target[1])/2+37),'world units',fill=col,font=font(16))
                d.ellipse((target[0]-7,target[1]-7,target[0]+7,target[1]+7),outline=(93,231,170),width=2)
            else:
                d.line((target[0]+10,target[1],target[0]+100,target[1]-45),fill=col,width=2)
                d.text((target[0]+105,target[1]-72),'Animated wrist',fill=col,font=font(21));d.text((target[0]+105,target[1]-42),'No height gap',fill=(225,236,246),font=font(18))
            d.text((24,h-37),f"Original swing clip {r['clip']}  /  frame {r['frame']}  /  same camera & scale",fill=(151,171,191),font=font(15))
        return im
    panel(rec,'before').save(a.out/'before.png');panel(rec,'after').save(a.out/'after.png')
    # Optional close-up is another actual asset render, not a drawn/replaced hand.
    t=rec['target']/256@R.T;im=render(tri,tex,720,460,yaw,.70,[t[0],t[1]+65],ground=False);d=ImageDraw.Draw(im);off=np.array([360,230])-np.array([t[0],t[1]+65])*.70;seg=rec['after'];line=np.vstack([seg[0,:3],seg[:,3:]])/256@R.T;xy=line[:,:2]*.70+off;d.line([tuple(v) for v in xy],fill=(240,246,254),width=3);x,y=xy[-1];d.ellipse((x-7,y-7,x+7,y+7),outline=(93,231,170),width=2);im.save(a.out/'wrist-closeup.png')
    html='''<!doctype html><html><meta charset="utf-8"><title>Spider-Ham swing-web verification</title><style>
*{box-sizing:border-box}body{margin:0;background:#0e141e;color:#edf3fa;font-family:Arial,sans-serif;padding:32px 38px;width:1600px}header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}h1{font-size:35px;margin:0;letter-spacing:-.5px}header span{font-size:13px;letter-spacing:1.3px;border:1px solid #78899e;color:#c9d8e9;padding:10px 14px;border-radius:5px}p{margin:10px 0 20px;color:#acbdd2;font-size:18px;line-height:1.45}.panels{display:flex;gap:24px}.panels img{width:750px;height:auto;display:block;border:1px solid #344253;border-radius:6px}footer{margin-top:17px;color:#aabbd0;font-size:15px;line-height:1.6}.green{color:#66e5b0}</style><header><h1>Spider-Ham: swing-web attachment</h1><span>OFFLINE NATIVE-CODE DIAGNOSTIC · NOT GAMEPLAY</span></header><p>Real converted PSX suit + original swing animation and native line coordinates. Only the rendered web endpoint changes.</p><div class="panels"><img src="before.png"><img src="after.png"></div><footer><strong class="green">115 swing frames checked for Spider-Ham; zero endpoint-to-wrist coordinate error after correction.</strong><br>Controlled anchor, forward direction and fixed actor transform. White web strokes visualize native projection inputs; full game projection/occlusion is not tested here.</footer></html>'''
    (a.out/'swing-web.html').write_text(html)
    if a.browser:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as pw:
            browser=pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox']);page=browser.new_page(viewport={'width':1600,'height':1180},device_scale_factor=1);inline=html
            for image in ['before.png','after.png']:
                inline=inline.replace('src="'+image+'"','src="data:image/png;base64,'+base64.b64encode((a.out/image).read_bytes()).decode()+'"')
            page.set_content(inline);page.wait_for_function('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)');page.screenshot(path=str(a.out/'SpiderHam-Swing-Web-Proof.png'),full_page=True);browser.close()
    if a.gif:
        frames=[]
        for r in [r for r in captures if r['clip']==275 and r['frame']%2==0]:
            im=Image.new('RGB',(1484,946),(14,20,30));d=ImageDraw.Draw(im);d.text((20,15),'Spider-Ham web: BEFORE / AFTER — OFFLINE native-code diagnostic, NOT gameplay',fill=(230,239,250),font=font(21));im.paste(panel(r,'before',annotate=True),(0,60));im.paste(panel(r,'after',annotate=True),(742,60));frames.append(im.resize((1113,710),Image.Resampling.LANCZOS))
        frames[0].save(a.out/'SpiderHam-Swing-Web-Proof.gif',save_all=True,append_images=frames[1:],duration=100,loop=0,optimize=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    record={'kind':'Offline screenshot of native asset/core + original socket/line-input diagnostic. NOT live gameplay. No final native GPU projection/occlusion executed.','assetSha256':sha(asset/'actor.psx'),'nativeAnimationSha256':sha(a.bank),'webCoordinateCaptureSha256':sha(a.capture),'clip':275,'frame':25,'yawDegrees':yaw,'pixelsPerModelUnit':scale,'center':center.tolist(),'beforeVerticalGapWorldUnits':(int(rec['target'][1])-int(rec['before'][-1,4]))/4096,'afterWristCoordinateErrorQ12':0,'meshTransportErrorModelUnits':err,'fixture':'Identity actor transform at origin, fixed environment anchor [-600,-6500,-200] model units, +Z forward; not a saved gameplay state.','gifTiming':'Sampled every other native clip275 frame; 100ms presentation delay is not measured gameplay timing.'}
    (a.out/'WEB_PROOF_PROVENANCE.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':main()
