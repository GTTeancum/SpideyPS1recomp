#!/usr/bin/env python3
"""Resumable small-batch SMU conversion, using the authoritative ready catalogue.

Never overwrite FBX/source textures or replace failed suits with simplified ones.
Each success is hash-checked and journalled before moving to the next costume.
The existing Repair06 converter supports single-mesh/single-diffuse suits;
unsupported scenes remain pending for a later converter update, not 'complete'.
"""
from __future__ import annotations
import argparse,contextlib,csv,hashlib,json,os,re,sys,traceback
from pathlib import Path
from PIL import Image
from convert import convert
from fbx_binary import Fbx

def sha(path:Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()
def save_json(path:Path,value):
    path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n');os.replace(tmp,path)

# The supplied exporter explicitly documents this authored filename alias.
TEXTURE_ALIASES={'_099_new_d':'2099_new_d'}
DISPLAY={'1602':'SMU 1602','2099':'SMU 2099','2099_new':'SMU 2099 NEW',
 'ben_reilly_unmasked':'SMU BEN UNMASKED','bigtime':'SMU BIG TIME','bigtime_red':'SMU BIG TIME RED',
 'gwen':'SMU SPIDER-GWEN','gwenom':'SMU GWENOM','noir':'SMU NOIR','spiderham':'SMU SPIDER-HAM'}

def inventory(samples:Path):
    rows=list(csv.DictReader((samples/'costume-catalogue.csv').open(encoding='utf-8-sig')))
    exported={x['name']:x for x in json.loads((samples/'costume-export-report.json').read_text())}
    ready=json.loads((samples/'costumes.json').read_text());assert {r['Costume'] for r in rows}=={r['name'] for r in ready}==set(exported)
    png={}
    for p in (samples/'textures-png').glob('*.png'):
        k=p.stem.casefold()
        if k in png:raise ValueError('Case-ambiguous texture: '+k)
        png[k]=p
    result=[]
    for row in rows:
        key=row['Costume'];fbx=samples/row['FBX'];r=exported[key]
        if not fbx.is_file():raise ValueError('Missing ready-catalogue FBX: '+key)
        authored=[x.strip() for x in row['Diffuse textures'].split(';') if x.strip()]
        if authored!=r['diffuse_textures']:raise ValueError('Catalogue/report texture disagreement: '+key)
        textures=[]
        for tex in authored:
            stem=Path(tex).stem.casefold();resolved=TEXTURE_ALIASES.get(stem,stem)
            if resolved not in png:raise ValueError('Unresolved authored diffuse: '+tex)
            textures.append(str(png[resolved].relative_to(samples)))
        result.append(dict(key=key,id='smu-'+key.replace('_','-'),fbx=row['FBX'],fbxSha256=sha(fbx),
            textureFiles=textures,textureSha256=[sha(samples/p) for p in textures],
            sourceMeshCount=r['meshes'],sourceVertices=int(row['Vertices']),sourceTriangles=int(row['Triangles']),
            sourceBoneCount=int(row['Bones']),authoredDiffuseNames=authored,status='pending'))
    return result

def material_policy(fbx:Path,texture:Path):
    """Follow authored FBX material connections, not raw diffuse-alpha guesses."""
    f=Fbx(fbx);defaults={};definitions=f.root.child('Definitions')
    if definitions:
        for n in definitions.all('ObjectType'):
            if n.props and n.props[0]=='Material':
                for t in n.all('PropertyTemplate'):defaults.update(t.properties())
    materials={k:n for k,n in f.objects.items() if n.name=='Material'}
    if not materials:raise ValueError('Missing authored material')
    records=[]
    for mid,n in materials.items():
        properties=dict(defaults);properties.update(n.properties())
        factor=float(properties.get('TransparencyFactor',[0.0])[0]);opacity=float(properties.get('Opacity',[1.0])[0])
        if factor!=0.0 or opacity!=1.0:raise ValueError('Non-opaque FBX material requires explicit blend handling')
        links=[c for c in f.connections if c[0]=='OP' and c[2]==mid]
        if any('transparen' in str(c[3]).casefold() or 'opacity' in str(c[3]).casefold() for c in links):
            raise ValueError('Opacity/transparent texture connection requires explicit handling')
        diffuse=[f.objects[c[1]] for c in links if c[3]=='DiffuseColor' and f.objects[c[1]].name=='Texture']
        if len(diffuse)!=1:raise ValueError('Expected one authored diffuse per FBX material')
        relative=str(diffuse[0].value('RelativeFilename') or diffuse[0].value('FileName')).replace(chr(92),'/')
        if Path(relative).name.casefold()!=texture.name.casefold():raise ValueError('FBX diffuse binding differs from the catalogue')
        records.append(dict(material=str(n.props[1]).split(chr(0))[0],transparencyFactor=factor,opacity=opacity,diffuse=Path(relative).name))
    with Image.open(texture) as im:alpha=im.getchannel('A').getextrema() if 'A' in im.getbands() else (255,255)
    return dict(mode='opaque diffuse RGB, matching FBX material connections and Repair06',materials=records,
        sourceAlphaRange=list(alpha),sourceAlphaBoundAsTransparency=False,sourceFileUnchanged=True)

def run(args):
    rows=inventory(args.samples);bykey={r['key']:r for r in rows};keys=args.keys
    if len(keys)!=len(set(keys)) or any(k not in bykey for k in keys):raise ValueError('Duplicate/unknown costume key')
    report=args.out/'batch-progress.json';args.out.mkdir(parents=True,exist_ok=True)
    state={'schema':1,'batch':args.batch,'scope':'SM1 native assets/core; .NET game not built/tested',
           'reference':'costumes/2099/2099.fbx','referenceSha256':sha(args.samples/'costumes/2099/2099.fbx'),
           'donorSha256':sha(args.donor),'requested':keys,'results':[]}
    if report.exists():
        prev=json.loads(report.read_text())
        if any(prev.get(k)!=state[k] for k in ['schema','referenceSha256','donorSha256','requested','batch']):raise ValueError('Resume identity differs')
        state=prev
    done={r['key']:r for r in state['results'] if r['status']=='converted'}
    save_json(args.out/'ready-catalogue-inventory.json',rows)
    for key in keys:
        row=bykey[key];dest=args.out/'suits'/row['id'];log=args.out/'logs'/(key+'.log');log.parent.mkdir(exist_ok=True)
        if key in done:
            if all(sha(dest/p)==h for p,h in done[key]['outputs'].items()) and row['fbxSha256']==done[key]['fbxSha256'] and row['textureSha256']==done[key]['textureSha256']:
                print('RESUME VERIFIED '+key,flush=True);continue
            raise ValueError('Resume output/source changed: '+key)
        if dest.exists() and any(dest.iterdir()):raise ValueError('Unjournalled existing output: '+str(dest))
        result=dict(row)
        try:
            if row['sourceMeshCount']!=1 or len(row['textureFiles'])!=1:raise ValueError('Requires multi-mesh/material conversion; no geometry or texture is dropped')
            tex=args.samples/row['textureFiles'][0]
            result['materialPolicy']=material_policy(args.samples/row['fbx'],tex)
            if key not in DISPLAY:raise ValueError('Needs an explicit <=19 character selector display name')
            with log.open('w') as f,contextlib.redirect_stdout(f):
                converted=convert(args.samples/row['fbx'],args.samples/state['reference'],args.donor,tex,dest,row['id'],DISPLAY[key])
            result.update(status='converted',sourceRigNodes=converted['sourceRigBones'],nativeBytes=converted['nativeBytes'],
                outputs={str(p.relative_to(dest)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()})
            print('CONVERTED '+key+': '+str(converted['originalTriangles'])+' triangles, '+str(converted['sourceRigBones'])+' rig nodes',flush=True)
        except (ValueError,OSError,AssertionError) as e:
            result.update(status='blocked',reason=str(e));print('BLOCKED '+key+': '+str(e),flush=True)
            log.write_text(log.read_text()+'\n'+traceback.format_exc() if log.exists() else traceback.format_exc())
        state['results']=[r for r in state['results'] if r['key']!=key]+[result];save_json(report,state)
    return 0 if all(r['status']=='converted' for r in state['results']) else 2

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--samples',type=Path,required=True);ap.add_argument('--donor',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--batch',required=True);ap.add_argument('--keys',nargs='+',required=True)
    args=ap.parse_args()
    try:return run(args)
    except (ValueError,OSError,AssertionError) as e:print('ERROR: '+str(e),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
