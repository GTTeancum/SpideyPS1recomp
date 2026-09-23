#!/usr/bin/env python3
"""Build/verify the in-process SFD decoder. No decoder download or system install.
Requires clang++ (both targets) and lld-link (win-x64). Runtime needs neither.
"""
from __future__ import annotations
import argparse, hashlib, json, os, platform, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent/'native'
SOURCES=['sfd_decoder.cpp','sfd_decoder.h','mpeg1_vlc.h','make_vlc_tables.py']
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--target',choices=['all','linux-x64','win-x64'],default='all')
 p.add_argument('--verify',action='store_true',help='verify existing binaries and their exact source provenance; do not compile')
 a=p.parse_args();targets=['linux-x64','win-x64'] if a.target=='all' else [a.target]
 out=HERE/'bin';manifest=out/'build-provenance.json'
 previous=json.loads(manifest.read_text()) if manifest.exists() else {'abi':'0x00010000','targets':{}}
 if a.verify:
  for target in targets:
   entry=previous['targets'][target]
   for f in entry['files']:
    if sha(ROOT/f['path'])!=f['sha256']:raise RuntimeError('SFD native/source mismatch: '+f['path'])
  print('SFD ABI/source/binary provenance verified: '+', '.join(targets));return
 compiler=shutil.which('clang++')
 if not compiler:raise RuntimeError('clang++ is required to rebuild the SFD component')
 common=['-O2','-std=c++17','-fno-exceptions','-fno-rtti','-fno-math-errno','-ffp-contract=off','-fno-stack-protector','-fno-builtin','-ffreestanding']
 out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='sfd-build-') as temp:
  temp=Path(temp)
  for target in targets:
   commands=[]
   if target=='linux-x64':
    if platform.system()!='Linux' or platform.machine().lower() not in ['x86_64','amd64']:raise RuntimeError('build Linux SFD on Linux x64, or use --verify for shipped ELF')
    final=out/'libOpenSpideySfd.so';binary=temp/final.name
    commands=[[compiler,*common,'-shared','-fPIC','-fvisibility=hidden','-nostdlib','-Wl,-z,defs',str(HERE/'sfd_decoder.cpp'),'-o',str(binary)]]
   else:
    linker=shutil.which('lld-link')
    if not linker:raise RuntimeError('lld-link is required for win-x64')
    final=out/'OpenSpideySfd.dll';binary=temp/final.name;obj=temp/'sfd.obj'
    commands=[[compiler,'--target=x86_64-pc-windows-msvc',*common,'-c',str(HERE/'sfd_decoder.cpp'),'-o',str(obj)],
              [linker,'/dll','/noentry','/nodefaultlib','/machine:x64','/out:'+str(binary),str(obj)]]
   for command in commands:print('+ '+' '.join(command),flush=True);subprocess.run(command,check=True)
   os.replace(binary,final)
   files=[HERE/name for name in SOURCES]+[final]
   if target=='win-x64':
    deploy=ROOT/'tools/RecompOne/native/win-x64/OpenSpideySfd.dll';deploy.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(final,deploy);files.append(deploy)
   previous['targets'][target]={'commands':commands,'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
    'files':[{'path':str(f.relative_to(ROOT)),'sha256':sha(f),'bytes':f.stat().st_size} for f in files]}
 manifest.write_text(json.dumps(previous,indent=2)+'\n')
 print('Native component built; this is not a complete game build.')
if __name__=='__main__':main()
