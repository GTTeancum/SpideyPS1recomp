#!/usr/bin/env python3
"""Extract original Spider-Man Dreamcast SFDs from a user's unpacked CUE/GDI disc.

This is MEDIA INSTALLATION, not a movie decoder or a claimed game-playback hook.
PS1 STR files are never replaced/deleted. The manifest records only observed
same-basename story/logo mappings; the PS1 LOGO.STR is deliberately not guessed.
Only ISO9660 files in data tracks are read. No mounting, SDK, or extra packages.
"""
from __future__ import annotations
import argparse
from contextlib import ExitStack
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import tempfile

SYNC = b"\0" + b"\xff" * 10 + b"\0"
STORY = tuple("L" + str(level) + "M" + str(movie)
              for level, count in [(1,2),(2,3),(3,1),(4,2),(5,4),(6,1),(7,3),(8,5)]
              for movie in range(1, count + 1))
PS1_MATCHES = frozenset(STORY + ("ATVILOGO",))

def bcd(value: int, max_tens: int = 9) -> int:
    if (value & 15) > 9 or (value >> 4) > max_tens:
        raise ValueError("invalid BCD raw-sector address")
    return (value >> 4) * 10 + (value & 15)

def raw_lba(header: bytes) -> int:
    if len(header) < 16 or header[:12] != SYNC or header[15] != 1:
        raise ValueError("expected a MODE1/2352 sector header")
    # The supplied GD-ROM continues past 99 minutes (A0...C2 minute bytes).
    # Only the minute tens nibble is extended; seconds/frames stay ordinary BCD.
    minute = bcd(header[12], max_tens=15)
    second, frame = map(bcd, header[13:15])
    if second >= 60 or frame >= 75: raise ValueError("invalid raw-sector MSF address")
    return (minute * 60 + second) * 75 + frame - 150

def safe_input(root: Path, name: str) -> Path:
    # Both Windows-style separators and parent escapes are refused on either OS.
    if not name or '\\' in name or ':' in name or Path(name).is_absolute():
        raise ValueError("unsafe track path: " + name)
    p = (root / name).resolve(strict=True)
    if not p.is_file() or not p.is_relative_to(root.resolve()):
        raise ValueError("track path leaves the disc directory")
    return p

@dataclass(frozen=True)
class Track:
    path: Path
    start: int
    sectors: int
    sector_bytes: int
    byte_offset: int

    @property
    def end(self) -> int: return self.start + self.sectors

def parse_disc(path: Path) -> list[Track]:
    root = path.resolve().parent
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    tracks: list[Track] = []
    if path.suffix.lower() == '.gdi':
        if not lines or not lines[0].strip().isdigit(): raise ValueError("invalid GDI count")
        records = [shlex.split(line) for line in lines[1:] if line.strip()]
        if len(records) != int(lines[0]): raise ValueError("GDI count mismatch")
        for r in records:
            if len(r) != 6: raise ValueError("invalid GDI record")
            _, start, mode, stride, name, off = r
            if int(mode) != 4: continue
            start, stride, off = int(start), int(stride), int(off)
            if stride not in (2048, 2352) or start < 0 or off < 0: raise ValueError("unsupported GDI data layout")
            p = safe_input(root, name); size = p.stat().st_size - off
            if size <= 0 or size % stride: raise ValueError("truncated GDI track")
            if stride == 2352:
                with p.open('rb') as f: f.seek(off); header = f.read(16)
                if raw_lba(header) != start: raise ValueError("GDI LBA disagrees with raw-sector header")
            tracks.append(Track(p, start, size // stride, stride, off))
    elif path.suffix.lower() == '.cue':
        current_file = None; rows = []; current = None
        for line in lines:
            words = shlex.split(line, comments=False)
            if not words: continue
            key = words[0].upper()
            if key == 'FILE':
                if len(words) != 3 or words[2].upper() != 'BINARY': raise ValueError("only binary CUE tracks are supported")
                current_file = safe_input(root, words[1]); current = None
            elif key == 'TRACK':
                if current_file is None or len(words) != 3: raise ValueError("TRACK without FILE")
                current = {'path':current_file, 'mode':words[2].upper(), 'indexes':{}}
                rows.append(current)
            elif key == 'INDEX':
                if current is None or len(words) != 3 or not re.fullmatch(r'\d+:\d{2}:\d{2}',words[2]): raise ValueError("invalid CUE INDEX")
                minute,second,frame = map(int,words[2].split(':'))
                if second >= 60 or frame >= 75: raise ValueError("invalid CUE timestamp")
                idx = int(words[1])
                if idx in current['indexes']: raise ValueError("duplicate CUE INDEX")
                current['indexes'][idx] = (minute*60+second)*75+frame
        for i,row in enumerate(rows):
            if row['mode'] == 'AUDIO': continue
            if row['mode'] != 'MODE1/2352': raise ValueError("CUE data needs raw MODE1/2352 headers for authoritative absolute LBAs; use GDI for 2048-byte tracks")
            if 1 not in row['indexes']: raise ValueError("data TRACK lacks INDEX 01")
            p = row['path']; size = p.stat().st_size
            if size % 2352: raise ValueError("truncated raw track")
            first = row['indexes'][1]; end = size // 2352
            for later in rows[i+1:]:
                if later['path'] == p:
                    index = later['indexes'].get(0, later['indexes'].get(1))
                    if index is None or index < first: raise ValueError("invalid shared-file CUE ordering")
                    end = min(end, index); break
            if first < 0 or end <= first: raise ValueError("empty CUE data range")
            with p.open('rb') as f: f.seek(first*2352); start = raw_lba(f.read(16))
            if start < 0: raise ValueError("negative data LBA")
            tracks.append(Track(p, start, end-first, 2352, first*2352))
    else:
        raise ValueError("supply an unpacked .cue or .gdi descriptor")
    tracks.sort(key=lambda t: t.start)
    if not tracks: raise ValueError("no supported data tracks")
    if any(a.end > b.start for a,b in zip(tracks, tracks[1:])): raise ValueError("overlapping data-track address ranges")
    return tracks

class Disc:
    def __init__(self, tracks: list[Track]):
        self.tracks = tracks; self.stack = ExitStack()
        self.handles = {t.path:self.stack.enter_context(t.path.open('rb')) for t in tracks}
    def __enter__(self): return self
    def __exit__(self,*args): return self.stack.__exit__(*args)
    def chunks(self, lba: int, size: int):
        if lba < 0 or size < 0 or size > 2**31: raise ValueError("invalid ISO extent")
        remain = size
        while remain:
            track = next((t for t in self.tracks if t.start <= lba < t.end), None)
            if track is None: raise ValueError(f"ISO extent enters an absent/audio track at LBA {lba}")
            sectors = min(128, track.end-lba, (remain+2047)//2048)
            f = self.handles[track.path]; f.seek(track.byte_offset+(lba-track.start)*track.sector_bytes)
            block = f.read(sectors*track.sector_bytes)
            if len(block) != sectors*track.sector_bytes: raise ValueError("track changed/truncated during read")
            if track.sector_bytes == 2048:
                cooked = block
            else:
                for i in range(sectors):
                    if raw_lba(block[i*2352:i*2352+16]) != lba+i: raise ValueError("noncontiguous/raw-sector address mismatch")
                cooked = b''.join(block[i*2352+16:i*2352+2064] for i in range(sectors))
            take = min(remain,len(cooked)); yield cooked[:take]
            lba += sectors; remain -= take
    def read(self,lba:int,size:int) -> bytes: return b''.join(self.chunks(lba,size))

def both32(b:bytes,off:int) -> int:
    if len(b)<off+8: raise ValueError("truncated ISO both-endian value")
    a=int.from_bytes(b[off:off+4],'little'); c=int.from_bytes(b[off+4:off+8],'big')
    if a!=c: raise ValueError("ISO endian copies disagree")
    return a

def directory(disc:Disc,extent:int,size:int,prefix:str='',seen=None):
    if size <= 0 or size > 16*1024*1024: raise ValueError("directory length out of bounds")
    if seen is None: seen=set()
    if extent in seen or len(seen)>256: raise ValueError("directory cycle/count limit")
    seen.add(extent); b=disc.read(extent,size); off=0; count=0
    while off<len(b):
        length=b[off]
        if length==0: off=((off//2048)+1)*2048; continue
        if length<34 or off+length>len(b) or off//2048!=(off+length-1)//2048: raise ValueError("invalid ISO directory record")
        r=b[off:off+length]; off+=length
        name_len=r[32]
        if 33+name_len>len(r): raise ValueError("truncated ISO filename")
        name=r[33:33+name_len]
        if name in (b'\0',b'\1'): continue
        count+=1
        if count>10000: raise ValueError("directory entry limit")
        name=name.decode('ascii').split(';')[0]
        if not name or name in ('.','..') or any(c in name for c in '/\\:'): raise ValueError("unsafe ISO name")
        if r[1]!=0 or r[25]&0x80 or r[26] or r[27]: raise ValueError("extended/interleaved/multi-extent records are unsupported")
        extent=both32(r,2); length=both32(r,10); full=prefix+name
        if r[25]&2:
            yield from directory(disc,extent,length,full+'/',seen)
        else:
            yield {'iso_path':full,'lba':extent,'bytes':length}

def find_movies(disc:Disc):
    candidates=[]
    for t in disc.tracks:
        if t.sectors <= 16: continue
        pvd=disc.read(t.start+16,2048)
        if pvd[:7]!=b'\x01CD001\x01': continue
        if pvd[128:132]!=b'\x00\x08\x08\x00': raise ValueError("unsupported ISO sector size")
        if pvd[40:72].rstrip(b' ')!=b'SMDC1': continue
        root=pvd[156:190]; entries=list(directory(disc,both32(root,2),both32(root,10)))
        movies=[e for e in entries if e['iso_path'].upper().endswith('.SFD')]
        if movies: candidates.append((t.start,movies))
    if len(candidates)!=1: raise ValueError(f"expected one Spider-Man filesystem containing SFDs; found {len(candidates)}")
    start,movies=candidates[0]; names=[Path(e['iso_path']).name.upper() for e in movies]
    if len(names)!=len(set(names)): raise ValueError("ambiguous duplicate SFD basenames")
    if not set(s+'.SFD' for s in STORY).issubset(names): raise ValueError("the disc lacks part of the observed 21-movie Spider-Man story set")
    return start, sorted(movies,key=lambda e:e['iso_path'])

def install(disc_path:Path,output:Path|None):
    tracks=parse_disc(disc_path)
    with Disc(tracks) as disc:
        session,movies=find_movies(disc)
        result={'schema':1,'media':'original-dreamcast-sfd','game':'Spider-Man PS1 (SM1)',
                'playback_implemented_by_this_tool':False,'ps1_str_fallback_untouched':True,
                'descriptor_sha256':hashlib.sha256(disc_path.read_bytes()).hexdigest(),
                'iso_volume':'SMDC1','data_session_lba':session,'movies':movies,
                'unmapped_ps1_movies':['LOGO.STR'],
                'tracks':[{'name':t.path.name,'start_lba':t.start,'data_sectors':t.sectors,'file_offset':t.byte_offset} for t in tracks]}
        if output is None: return result
        output=output.resolve()
        if output.exists(): raise ValueError("output must not already exist; existing data is never overwritten")
        # Output must not overlap the source descriptor/track directory.
        if disc_path.resolve().is_relative_to(output) or any(t.path.is_relative_to(output) for t in tracks):
            raise ValueError("output overlaps original disc inputs")
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.sfd-stage-',dir=output.parent) as tmp:
            stage=Path(tmp)
            for entry in movies:
                name=Path(entry['iso_path']).name.upper(); target=stage/name
                h=hashlib.sha256(); first=True
                with target.open('xb') as f:
                    for block in disc.chunks(entry['lba'],entry['bytes']):
                        if first and block[:4]!=b'\0\0\x01\xba': raise ValueError(name+': not an MPEG-PS/Sofdec pack stream')
                        first=False;h.update(block);f.write(block)
                if first: raise ValueError('empty SFD: '+name)
                entry['file']=name;entry['sha256']=h.hexdigest()
                stem=Path(name).stem
                entry['ps1_movie']=stem+'.STR' if stem in PS1_MATCHES else None
            (stage/'sfd-install.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
            (stage/'README.txt').write_text('Original Dreamcast SFD media installed without altering PS1 STR files.\n'
                'This installer does not enable/claim SFD playback in the game.\n'
                'Use the existing PS1 videos until a separately verified runtime playback path is available.\n',encoding='utf8')
            # os.rename is same-volume; on Windows it refuses an existing destination.
            # Linux rename can replace an empty directory, so explicitly recheck.
            if output.exists(): raise ValueError('output appeared during installation')
            os.rename(stage,output)
        return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('disc',type=Path,help='unpacked user-owned CUE or GDI')
    ap.add_argument('--output',type=Path,help='new output directory; omit for a read-only inventory')
    args=ap.parse_args()
    try: result=install(args.disc,args.output)
    except (OSError,ValueError,UnicodeError,struct.error) as exc: ap.exit(1,'SFD installation failed: '+str(exc)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
