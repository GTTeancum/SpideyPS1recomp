#!/usr/bin/env python3
"""Resumable small-batch SMU conversion, using the authoritative ready catalogue.

Never overwrite FBX/source textures or replace failed suits with simplified ones.
Each success is hash-checked and journalled before moving to the next costume.
Unsupported scenes remain pending, never substituted or marked complete.
"""
from __future__ import annotations
import argparse,contextlib,csv,hashlib,json,os,re,sys,traceback
from pathlib import Path
from convert import convert
from scene import Scene
from materials import bindings

def sha(path:Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()

def selector_names(rows):
    names={};used=set()
    for number,row in enumerate(rows,1):
        key=row['key'];text=DISPLAY.get(key,'SMU '+key.replace('_',' ').upper())
        if len(text)>19:text=key.replace('_',' ').upper()
        if len(text)>19 or text in used:text=text[:14].rstrip()+f' #{number:03}'
        if len(text)>19 or text in used:raise ValueError('Selector name collision: '+key)
        names[key]=text;used.add(text)
    return names
def save_json(path:Path,value):
    path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n');os.replace(tmp,path)

# The supplied exporter explicitly documents this authored filename alias.
TEXTURE_ALIASES={'_099_new_d':'2099_new_d'}
DISPLAY={'1602':'SMU 1602','2099':'SMU 2099','2099_new':'SMU 2099 NEW',
 'ben_reilly_unmasked':'SMU BEN UNMASKED','bigtime':'SMU BIG TIME','bigtime_red':'SMU BIG TIME RED',
 'gwen':'SMU SPIDER-GWEN','gwenom':'SMU GWENOM','noir':'SMU NOIR','spiderham':'SMU SPIDER-HAM',
 'venom':'SMU VENOM','poisonrhino':'SMU POISON RHINO','venom_2099':'SMU VENOM 2099',
 'scarlet_spiderham':'SMU SCARLET HAM','2211':'SMU 2211'}

def inventory(samples:Path):
    repair_path=Path(__file__).with_name('SOURCE-REPAIRS.json')
    repairs=json.loads(repair_path.read_text()) if repair_path.exists() else {}
    project=Path(__file__).resolve().parents[2]
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
        repair=repairs.get(key)
        if repair:
            if sha(fbx)!=repair['originalFbxSha256'] or sha(samples/repair['bdae'])!=repair['bdaeSha256']:
                raise ValueError('Source repair original identity changed: '+key)
            fbx=(project/repair['fbx']).resolve()
            if not fbx.is_relative_to(project) or sha(fbx)!=repair['fbxSha256']:
                raise ValueError('Source repair derived identity changed: '+key)
        authored=[x.strip() for x in row['Diffuse textures'].split(';') if x.strip()]
        if authored!=r['diffuse_textures']:raise ValueError('Catalogue/report texture disagreement: '+key)
        textures=[]
        for tex in authored:
            stem=Path(tex).stem.casefold();resolved=TEXTURE_ALIASES.get(stem,stem)
            if resolved not in png:raise ValueError('Unresolved authored diffuse: '+tex)
            textures.append(str(png[resolved].relative_to(samples)))
        result.append(dict(key=key,id='smu-'+key.replace('_','-'),fbx=str(fbx) if repair else row['FBX'],fbxSha256=sha(fbx),
            textureFiles=textures,textureSha256=[sha(samples/p) for p in textures],
            sourceMeshCount=r['meshes'],sourceVertices=repair['vertices'] if repair else int(row['Vertices']),sourceTriangles=repair['triangles'] if repair else int(row['Triangles']),
            sourceBoneCount=int(row['Bones']),authoredDiffuseNames=authored,status='pending'))
    return result

def material_policy(fbx:Path,texture:Path):
    """Follow authored FBX material connections, not raw diffuse-alpha guesses."""
    records,images,indices,uv=bindings(Scene(fbx),texture)
    return dict(mode='Authored per-face diffuse bindings; source alpha only where connected; explicit repeat tiles',
        materials=records,sourceFileUnchanged=True)

def run(args):
    rows=inventory(args.samples);bykey={r['key']:r for r in rows};keys=args.keys;names=selector_names(rows)
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
            tex=args.samples/row['textureFiles'][0]
            result['materialPolicy']=material_policy(args.samples/row['fbx'],tex)
            with log.open('w') as f,contextlib.redirect_stdout(f):
                converted=convert(args.samples/row['fbx'],args.samples/state['reference'],args.donor,tex,dest,row['id'],names[key])
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
