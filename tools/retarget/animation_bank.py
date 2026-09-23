"""Read the proof oracle's exact native 24-byte pose matrices."""
import struct
from pathlib import Path
import numpy as np

def read(path):
    d=Path(path).read_bytes()
    if d[:4]!=b'ANM2':raise ValueError('Expected oracle animation output')
    count=struct.unpack_from('<I',d,4)[0];p=8;clips=[]
    for _ in range(count):
        ix,n=struct.unpack_from('<II',d,p);p+=8
        raw=np.frombuffer(d,dtype='<i2',count=n*18*12,offset=p).reshape(n,18,12);p+=n*18*24
        mats=np.zeros((n,18,3,4),np.float32);mats[:,:,:,:3]=raw[:,:,:9].reshape(n,18,3,3)/4096;mats[:,:,:,3]=raw[:,:,9:]
        clips.append(mats)
    if p!=len(d):raise ValueError('Trailing animation data')
    return clips
