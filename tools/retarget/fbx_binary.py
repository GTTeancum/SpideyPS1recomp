"""Read-only, bounded binary-FBX scene reader for native suit conversion.

No Autodesk/Blender SDK required. Does not silently discard properties or skin
influences. Scene-level transform compatibility is checked during calibration.
"""
from __future__ import annotations
import dataclasses, struct, zlib
from pathlib import Path
import numpy as np

@dataclasses.dataclass
class Node:
    name: str
    props: list
    children: list
    def child(self, name):
        return next((c for c in self.children if c.name == name), None)
    def all(self, name):
        return [c for c in self.children if c.name == name]
    def value(self, name, default=None):
        c=self.child(name)
        return c.props[0] if c and c.props else default
    def properties(self):
        p=self.child('Properties70')
        return {c.props[0]:c.props[4:] for c in p.children if c.name=='P'} if p else {}

class Fbx:
    def __init__(self, path):
        self.data=Path(path).read_bytes()
        if not self.data.startswith(b'Kaydara FBX Binary  \x00\x1a\x00'):
            raise ValueError('Expected a binary FBX')
        self.version=struct.unpack_from('<I',self.data,23)[0]
        if not 7000 <= self.version <= 7700: raise ValueError('Unsupported FBX version')
        self.header=25 if self.version>=7500 else 13
        self.nodes=self._nodes(27,len(self.data),0)
        self.root=Node('root',[],self.nodes)
        objects=self.root.child('Objects')
        if not objects: raise ValueError('Missing FBX Objects')
        self.objects={n.props[0]:n for n in objects.children if n.props}
        con=self.root.child('Connections')
        self.connections=[n.props for n in con.children if n.name=='C'] if con else []
        self.parent={c[1]:c[2] for c in self.connections if c[0]=='OO' and c[1] in self.objects and self.objects[c[1]].name=='Model' and (c[2]==0 or (c[2] in self.objects and self.objects[c[2]].name=='Model'))}
    def _need(self,p,n):
        if p<0 or n<0 or p+n>len(self.data): raise ValueError('Truncated FBX')
    def _prop(self,p):
        self._need(p,1);t=chr(self.data[p]);p+=1
        fm={'Y':'h','C':'?','I':'i','F':'f','D':'d','L':'q'}
        if t in fm:
            n=struct.calcsize(fm[t]);self._need(p,n)
            return struct.unpack_from('<'+fm[t],self.data,p)[0],p+n
        if t in 'SR':
            self._need(p,4);n=struct.unpack_from('<I',self.data,p)[0];p+=4;self._need(p,n)
            b=self.data[p:p+n]
            return (b.decode('utf8') if t=='S' else b),p+n
        if t in 'fdilbc':
            self._need(p,12);n,enc,size=struct.unpack_from('<III',self.data,p);p+=12
            dt=np.dtype({'f':'<f4','d':'<f8','i':'<i4','l':'<i8','b':'u1','c':'u1'}[t]);wanted=n*dt.itemsize
            if wanted>128*1024*1024 or enc not in (0,1): raise ValueError('Invalid FBX array')
            self._need(p,size);raw=self.data[p:p+size]
            if enc:
                dec=zlib.decompressobj(); raw=dec.decompress(raw,wanted+1)
                if not dec.eof or dec.unconsumed_tail or len(raw)!=wanted: raise ValueError('Invalid compressed FBX array')
            if len(raw)!=wanted: raise ValueError('Invalid FBX array size')
            return np.frombuffer(raw,dt).copy(),p+size
        raise ValueError(f'Unsupported FBX property {t!r}')
    def _nodes(self,p,limit,depth):
        if depth>64: raise ValueError('FBX nesting too deep')
        out=[]
        while p+self.header<=limit:
            self._need(p,self.header)
            fmt='<QQQB' if self.header==25 else '<IIIB'
            end,num,size,nl=struct.unpack_from(fmt,self.data,p)
            if end==0: break
            if not p+self.header<=end<=limit or num>1000000: raise ValueError('Invalid FBX node extent')
            q=p+self.header;self._need(q,nl);name=self.data[q:q+nl].decode('utf8');q+=nl;start=q
            props=[]
            for _ in range(num):v,q=self._prop(q);props.append(v)
            if q-start!=size or q>end: raise ValueError('Invalid property extent')
            children=self._nodes(q,end-self.header,depth+1) if q<end-self.header else []
            out.append(Node(name,props,children));p=end
        return out

def clean(s): return s.split('\x00',1)[0].split('::')[-1]

if __name__=='__main__':
    import sys,collections
    f=Fbx(sys.argv[1]);print('FBX',f.version,collections.Counter(n.name for n in f.objects.values()))
    for key,n in f.objects.items():
        if n.name=='Model':print(key,clean(n.props[1]),n.props[2], 'parent',f.parent.get(key),n.properties())
        elif n.name=='Geometry':print('GEOMETRY',key, [(c.name,len(c.props[0]) if c.props and isinstance(c.props[0],np.ndarray) else c.props) for c in n.children])
        elif n.name=='Deformer' and n.props[2]=='Cluster':
            print('CLUSTER',key,len(n.value('Indexes',[])),[(c[1],clean(f.objects[c[1]].props[1])) for c in f.connections if c[0]=='OO' and c[2]==key])
    print('connections',f.connections)
