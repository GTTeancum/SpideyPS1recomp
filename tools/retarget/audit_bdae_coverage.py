"""Blender-background audit of source graph mesh references versus imported bodies."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--samples',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
sys.path.insert(0,str(a.samples/'tools/vendor'))
from SMU_toolkit import bdae_core as core
repairs=json.loads(Path(__file__).with_name('SOURCE-REPAIRS.json').read_text())
costumes=json.loads((a.samples/'costumes.json').read_text())
catalogue={r['Costume']:r for r in csv.DictReader((a.samples/'costume-catalogue.csv').open(encoding='utf-8-sig'))}
groups={}
for costume in costumes:groups.setdefault(costume['mesh'],[]).append(costume['name'])
results=[]
for mesh,names in sorted(groups.items()):
    source=a.samples/'models'/Path(mesh).stem/mesh
    bres=core.BresFile_v2(core.BdaeContainer_v2.load(str(source)).inner_bytes)
    recognized={d.offset:d for d in bres.scan_mesh_descriptors()}
    refs=sorted((loc,value) for loc,value in bres.relocated_string_locations.items()
                if value.lower().endswith('-mesh') and not value.startswith('#'))
    clusters=[]
    for ref in refs:
        if not clusters or ref[0]-clusters[-1][-1][0]>0x20:clusters.append([])
        clusters[-1].append(ref)
    records=[];errors=[]
    for cluster in clusters:
        payloads=[];start=cluster[-1][0]+0x10
        for q in range(start,min(len(bres.data)-16,start+0x60+len(cluster)*0x30),4):
            payload=bres._read_payload_descriptor(q)
            if not payload:continue
            for target in (payload[1],payload[1]+8):
                if target+0x50>len(bres.data):continue
                size=core.load_u32(bres.data,target);count=core.load_u32(bres.data,target+12)
                vb=core.load_u32(bres.data,target+0x40);vs=core.load_u32(bres.data,target+0x48)
                if (0x80<=size<=0x300 and size%8==0 and core.load_u32(bres.data,target+4)==0
                    and core.load_u32(bres.data,target+8)==1 and 0<count<=1000000
                    and target+size<=vb<len(bres.data) and 0<vs<=len(bres.data)-vb):
                    if target not in payloads:payloads.append(target)
                    break
        if len(payloads)!=len(cluster):
            errors.append(f'Graph cluster has {len(cluster)} names but {len(payloads)} bounded mesh payloads')
            continue
        for (loc,value),target in zip(cluster,payloads):
            name=core.tidy_label(bres.find_string_at(core.load_u64(bres.data,loc+8)) or value[:-5])
            helper=name.lower().startswith(('spiderweb_mesh','webhammer'))
            desc=recognized.get(target)
            entry=dict(name=name,offset=target,helper=helper,recognized=desc is not None,
                       vertices=core.load_u32(bres.data,target+12))
            if desc:
                faces,_=core.load_faces(bres,desc,core.ReportLog_v2())
                entry.update(importerName=desc.name,triangles=len(faces))
                if desc.name!=name:errors.append('Importer name mismatch: '+name+' -> '+desc.name)
            elif not helper:
                repair=next((repairs[n] for n in names if n in repairs),None)
                if repair and hashlib.sha256(source.read_bytes()).hexdigest()==repair['bdaeSha256'] and target==30840:
                    entry.update(repaired=True,triangles=repair['triangles'])
                else:errors.append('Unrecognized body: '+name)
            records.append(entry)
    bodies=[r for r in records if not r['helper']]
    if not bodies:errors.append('No body mesh references')
    for name in names:
        expected=repairs.get(name)
        vertices=expected['vertices'] if expected else int(catalogue[name]['Vertices'])
        triangles=expected['triangles'] if expected else int(catalogue[name]['Triangles'])
        if sum(r['vertices'] for r in bodies)!=vertices or sum(r.get('triangles',0) for r in bodies)!=triangles:
            errors.append('Export body counts differ: '+name)
    row=dict(mesh=mesh,sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),suits=names,
             meshes=records,errors=errors,status='PASS' if not errors else 'FAIL')
    # Retain the stock importer's known failure; require the installed repair evidence.
    if names==['devilspider'] and set(errors)=={
        'Importer name mismatch: SpiderWeb_mesh_01 -> Mesh_DevilSpider',
        'Importer name mismatch: SpiderWeb_mesh_02 -> SpiderWeb_mesh_01'}:
        project=Path(__file__).resolve().parents[2];repair=repairs['devilspider']
        check=json.loads((project/'verification/source-repairs/devilspider/repeat-check.json').read_text())
        derived=project/repair['fbx']
        if (check['status']=='PASS' and check['registeredSha256']==repair['fbxSha256']
            and hashlib.sha256(derived.read_bytes()).hexdigest()==repair['fbxSha256']
            and row['sourceSha256']==repair['bdaeSha256']):
            row.update(status='REPAIRED',repairEvidence='verification/source-repairs/devilspider/repeat-check.json')
    results.append(row)
    if errors:print(json.dumps(row),flush=True)
report=dict(scope='Source graph descriptor and non-helper body-count coverage, not visual acceptance or a complete raw skin audit.',
            sourceFiles=len(results),suits=sum(len(r['suits']) for r in results),results=results,
            status='PASS' if results and all(r['status'] in ('PASS','REPAIRED') for r in results) else 'FAIL')
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n')
print(report['status'],report['sourceFiles'],'source files,',report['suits'],'suits')
if report['status']!='PASS':raise RuntimeError('Source coverage audit failed; inspect report')
