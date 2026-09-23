"""Blender-background export of the skipped Devil Spider BDAE body; originals stay untouched."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import struct

import bpy

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--samples',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
sys.path.insert(0,str(a.samples/'tools/vendor'))
from SMU_toolkit import bdae_core as core

source=a.samples/'models/Mesh_DevilSpider/Mesh_DevilSpider.bdae'
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
if source_hash!='5a5d5c250aeba44556ef6a235a20e218d6ac3e93c5975215ca493ec13784bdf6':
    raise ValueError('This scoped repair requires the audited original BDAE')
if a.out.exists():raise ValueError('Use a fresh output directory')

# 0x150 extends the same four attribute arrays to ten slots; the body uses nine.
original_layout=core.BresFile_v2._read_vertex_layout
def extended_layout(self,off,size,stride):
    if size!=0x150:return original_layout(self,off,size,stride)
    if stride!=92:raise ValueError('Unexpected extended body stride')
    offsets=[];types=[];components=[]
    for i in range(10):
        step=core.load_u32(self.data,off+0x80+i*4)
        offset=core.load_u32(self.data,off+0xa8+i*4)
        kind=core.load_u32(self.data,off+0xd0+i*4)
        width=core.load_u32(self.data,off+0xf8+i*4)
        if step==stride and 1<=width<=4 and offset<stride:
            offsets.append(offset);types.append(kind);components.append(width)
    if len(offsets)!=9:raise ValueError('Incomplete extended body layout')
    return offsets,types,components
core.VALID_DESCRIPTOR_SIZES=core.VALID_DESCRIPTOR_SIZES|{0x150}
core.VALID_VERTEX_STRIDES=core.VALID_VERTEX_STRIDES|{92}
core.BresFile_v2._read_vertex_layout=extended_layout
original_batches=core.BresFile_v2._read_primitive_batches
def extended_batches(self,off,size,count,vertex_buffer):
    if size!=0x150:return original_batches(self,off,size,count,vertex_buffer)
    q=off+size
    triangles=core.load_u32(self.data,q+8);kind=core.load_u32(self.data,q+12)
    maximum=core.load_u32(self.data,q+24);indices=core.load_u32(self.data,q+28)
    index_buffer=core.load_u32(self.data,q+40);index_bytes=core.load_u32(self.data,q+48)
    if (off,count,kind,triangles,indices,maximum)!=(30840,1819,9,3058,9174,1818):
        raise ValueError('Unexpected extended primitive identity')
    if core.load_u32(self.data,q+36)!=0x01000000 or not 0<=index_bytes-indices*2<=32:
        raise ValueError('Invalid index buffer encoding')
    if index_buffer<vertex_buffer or index_buffer+index_bytes>len(self.data):
        raise ValueError('Index buffer outside source')
    values=struct.unpack_from('<'+str(indices)+'H',self.data,index_buffer)
    if max(values)!=maximum or any(v>=count for v in values):raise ValueError('Invalid triangle references')
    material=core.tidy_label(core.load_c_txt(self.data,q+0x50,32))
    if material!='DevilSpider':raise ValueError('Unexpected body material')
    return [core.PrimBatch(offset=q,material_index=0,material_name=material,
        vertex_start=core.load_u32(self.data,q+20),max_index=maximum,index_count=indices,
        index_buffer_offset=index_buffer,index_buffer_size=index_bytes,material_record_offset=q+0x50)]
core.BresFile_v2._read_primitive_batches=extended_batches
bres=core.BresFile_v2(core.BdaeContainer_v2.load(str(source)).inner_bytes)
descriptors=bres.scan_mesh_descriptors()
print('Descriptors:',[(d.name,d.offset,d.vertex_count) for d in descriptors],flush=True)
body=next(d for d in descriptors if d.name=='Mesh_DevilSpider')
if body.offset!=30840 or body.vertex_count!=1819:raise ValueError('Body identity mismatch')
if {d.name for d in descriptors}!={'Mesh_DevilSpider','SpiderWeb_mesh_01','SpiderWeb_mesh_02'}:
    raise ValueError('Unexpected mesh set')
raw_faces,_=core.load_faces(bres,body,core.ReportLog_v2())
bpy.ops.wm.read_factory_settings(use_empty=True)
log=core.ReportLog_v2();core.ingest_bdae(str(source),bpy.context,log,pre_skin_meshes=True)
mesh=bpy.data.objects.get('Mesh_DevilSpider')
if mesh is None or mesh.type!='MESH' or len(mesh.data.vertices)!=1819:raise ValueError('Missing imported body')
if len(mesh.data.polygons)!=len(raw_faces):raise ValueError('Triangle loss on import')
if [tuple(poly.vertices) for poly in mesh.data.polygons]!=raw_faces:raise ValueError('Source triangle order changed')
skin,dense,aux=bres.matching_skin_for_mesh(body,bres.scan_skin_records())
if dense is None:raise ValueError('Missing original body skin')
raw_weights=core.interpret_dense_deform_wgts(bres.data,dense,body.vertex_count)
joint_names=json.loads(mesh['bdae_joint_name_map_json'])
weight_error=0.0
for vertex,expected in zip(mesh.data.vertices,raw_weights):
    original={}
    for joint,weight in expected:original[joint]=original.get(joint,0.0)+weight
    actual={joint_names[mesh.vertex_groups[g.group].name]:g.weight for g in vertex.groups}
    if actual.keys()!=original.keys():raise ValueError('Imported body influence identities changed')
    weight_error=max(weight_error,max(abs(actual[j]-weight) for j,weight in original.items()))
if weight_error>1e-6:raise ValueError('Imported body skin weights changed')
rigs=[o for o in bpy.data.objects if o.type=='ARMATURE']
if len(rigs)!=1:raise ValueError('Expected original shared rig')
materials=[]
for mat in mesh.data.materials:
    name=str(mat.get('bdae_diffuse_texture',''))
    if Path(name).name.lower()!='devilspider_d.tga':raise ValueError('Unexpected body diffuse: '+name)
    alpha=bool(mat.get('bdae_alpha_test',False))
    mat.use_nodes=True;bsdf=mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value=.85
    node=mat.node_tree.nodes.new('ShaderNodeTexImage')
    node.image=bpy.data.images.load(str(a.samples/'textures-png/DevilSpider_D.png'),check_existing=True)
    mat.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
    if alpha:mat.node_tree.links.new(node.outputs['Alpha'],bsdf.inputs['Alpha'])
    materials.append(dict(name=mat.name,diffuse=name,alphaBound=alpha))
bpy.ops.object.select_all(action='DESELECT')
for obj in [mesh,*rigs]:obj.select_set(True)
a.out.mkdir(parents=True)
target=a.out/'devilspider.fbx'
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'MESH','ARMATURE'},
    add_leaf_bones=False,bake_anim=False,path_mode='RELATIVE',use_custom_props=False)
report=dict(source=str(source),sourceSha256=source_hash,fbxSha256=hashlib.sha256(target.read_bytes()).hexdigest(),
    descriptorOffset=body.offset,descriptorBytes=body.desc_size,vertexStride=body.vertex_stride,
    vertices=len(mesh.data.vertices),triangles=len(mesh.data.polygons),bones=len(rigs[0].data.bones),
    materials=materials,importLog=log.ui_message(),maxRawSkinWeightError=weight_error,
    scope='Derived corrected export. Original BDAE, FBXs, toolkit and textures were not modified. Native conversion and gameplay acceptance remain separate.')
assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
(a.out/'source-repair.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
