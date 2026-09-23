"""Preserve authored diffuse bindings, alpha use and repeat UVs per material."""
from pathlib import Path
import numpy as np
from PIL import Image
from fbx_binary import clean


def bindings(scene, texture):
    if hasattr(scene,'parts'):
        records=[];images=[];indices=[];uv=[]
        for part in scene.parts:
            r,im,ix,u=bindings(part,texture);offset=len(records)
            records.extend(dict(record,slot=record['slot']+offset) for record in r)
            images.extend(im);indices.append(ix+offset);uv.append(u)
        return records,images,np.concatenate(indices),np.concatenate(uv)
    f=scene.fbx
    geometries=[k for k,n in f.objects.items() if n is scene.geometry]
    model=next(c[2] for c in f.connections if c[0]=='OO' and c[1]==geometries[0])
    ids=[c[1] for c in f.connections if c[0]=='OO' and c[2]==model and f.objects[c[1]].name=='Material']
    layer=scene.geometry.child('LayerElementMaterial')
    indices=np.zeros(len(scene.faces),dtype=int)
    if layer:
        mapping=layer.value('MappingInformationType');values=np.asarray(layer.value('Materials'),int)
        if mapping=='AllSame':indices[:]=values[0]
        elif mapping=='ByPolygon' and len(values)==len(indices):indices=values.copy()
        else:raise ValueError('Unsupported material layer mapping: '+str(mapping))
    if not ids or np.any(indices<0) or np.any(indices>=len(ids)):raise ValueError('Invalid source material index')
    defaults={}
    for node in f.root.child('Definitions').all('ObjectType'):
        if node.props and node.props[0]=='Material':
            for template in node.all('PropertyTemplate'):defaults.update(template.properties())
    png={p.name.casefold():p for p in texture.parent.glob('*.png')}
    records=[];images=[];uv=scene.uv.copy()
    for slot,mid in enumerate(ids):
        node=f.objects[mid];properties=dict(defaults);properties.update(node.properties())
        if float(properties.get('TransparencyFactor',[0])[0])!=0 or float(properties.get('Opacity',[1])[0])!=1:
            raise ValueError('Non-unit material opacity requires explicit handling')
        links=[c for c in f.connections if c[0]=='OP' and c[2]==mid]
        diffuse=[f.objects[c[1]] for c in links if c[3]=='DiffuseColor' and f.objects[c[1]].name=='Texture']
        if len(diffuse)!=1:raise ValueError('Material must have one diffuse binding')
        name=Path(str(diffuse[0].value('RelativeFilename') or diffuse[0].value('FileName')).replace('\\','/')).name
        source=png.get(name.casefold())
        if source is None:raise ValueError('Missing material diffuse: '+name)
        alpha=any('transparen' in str(c[3]).lower() or 'opacity' in str(c[3]).lower() for c in links)
        for c in links:
            if 'transparen' in str(c[3]).lower() or 'opacity' in str(c[3]).lower():
                opacity=f.objects[c[1]]
                opacity_name=Path(str(opacity.value('RelativeFilename') or opacity.value('FileName')).replace('\\','/')).name
                if opacity_name.casefold()!=name.casefold():
                    raise ValueError('Separate opacity image requires explicit channel mapping')
        selected=indices==slot
        lo=np.zeros(2,int);tiles=np.ones(2,int)
        if selected.any():
            minimum=uv[selected].min(axis=(0,1));maximum=uv[selected].max(axis=(0,1))
            if minimum.min()<-.001 or maximum.max()>1.001:
                lo=np.floor(minimum).astype(int);tiles=np.maximum(1,np.ceil(maximum).astype(int)-lo)
                if np.any(tiles>8):raise ValueError('Repeat texture exceeds eight tiles per axis')
                uv[selected]=(uv[selected]-lo)/tiles
        with Image.open(source) as original:im=original.convert('RGBA' if alpha else 'RGB')
        if np.any(tiles!=1):
            im=Image.fromarray(np.tile(np.asarray(im),(int(tiles[1]),int(tiles[0]),1)))
        records.append(dict(name=clean(node.props[1]),source=str(source.resolve()),alphaBound=alpha,
            tileOrigin=lo.tolist(),tileCount=tiles.tolist(),slot=slot))
        images.append(im)
    return records,images,indices,uv
