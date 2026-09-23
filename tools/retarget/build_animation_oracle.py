#!/usr/bin/env python3
"""Compile three supplied recompiled animation routines as a Linux test oracle.

This is NOT the running game. It executes the same decompression/matrix code
with bounded RAM and just the GTE MVMVA operation those functions require.
"""
from pathlib import Path
import argparse,re,subprocess
ap=argparse.ArgumentParser();ap.add_argument('--project',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
s=(a.project/'spiderman/generated/main.cs').read_text()
def extract(name):
 start=s.index('    public static void '+name+'(');op=s.index('{',start);depth=1;p=op+1
 while depth:
  depth+=int(s[p]=='{')-int(s[p]=='}');p+=1
 text=s[start:p]
 text=re.sub(r'public static void (\w+)\(CpuContext c, IMemory m\)',r'void \1(Context& c, Mem& m)',text)
 text=re.sub(r'\s*RecompOne.Runtime.Diagnostics.CallRing.Enter\([^;]+;', '', text)
 text=text.replace('var _r','auto _r').replace('int.MinValue','INT32_MIN').replace('SpiderMan.','')
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU(8|16)\(c, (\d+), m, (.*), (true|false)\);',lambda m:f'c.r[{m[2]}] = '+(f'(uint)(int)(int{m[1]}_t)' if m[4]=='true' else '')+f'm.ReadU{m[1]}({m[3]});',text)
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU32\(c, (\d+), m, (.*)\);',r'c.r[\1] = m.ReadU32(\2);',text)
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.StoreU32\(m, (.*), (.*), c.GetGteVertexTag\(\d+\)\);',r'm.WriteU32(\1, \2);',text)
 text=text.replace('RecompOne.Runtime.Gte.','Gte::').replace('Dispatcher.Call','Dispatch')
 if 'RecompOne.' in text:raise ValueError('Unsupported source construct remains')
 return text
preamble=r'''
#include <cstdint>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
using uint=uint32_t;using ushort=uint16_t;using byte=uint8_t;using ulong=uint64_t;
struct Context { union { uint r[32];struct {uint Zero,At,V0,V1,A0,A1,A2,A3,T0,T1,T2,T3,T4,T5,T6,T7,S0,S1,S2,S3,S4,S5,S6,S7,T8,T9,K0,K1,GP,SP,FP,RA;};};uint LO=0,HI=0;Context(){for(auto&x:r)x=0;SP=0x807ff000;}template<class...T>void SetDerived(int i,uint v,T...){r[i]=v;}};
struct Mem { std::vector<byte>b=std::vector<byte>(0x800000);uint offset(uint p,uint n){uint q=p&0x7fffff;if(q+n>b.size())throw std::runtime_error("RAM bounds");return q;}uint ReadU8(uint p){return b[offset(p,1)];}uint ReadU16(uint p){uint q=offset(p,2);return b[q]|(b[q+1]<<8);}uint ReadU32(uint p){return ReadU16(p)|(ReadU16(p+2)<<16);}void WriteU8(uint p,byte v){b[offset(p,1)]=v;}void WriteU16(uint p,ushort v){WriteU8(p,v);WriteU8(p+1,v>>8);}void WriteU32(uint p,uint v){WriteU16(p,v);WriteU16(p+2,v>>16);} };
namespace Gte {uint cr[32]={},dr[32]={};uint ReadControl(int i){return cr[i];}void WriteControl(int i,uint v){cr[i]=v;}void Write(int i,uint v){dr[i]=v;}void ReadTo(Context& c,int i,int d){c.r[i]=dr[d];}void Execute(uint op){if(op!=0x4a486012)throw std::runtime_error("Unexpected GTE opcode");int16_t mat[9];for(int j=0;j<9;j++)mat[j]=(int16_t)(cr[j/2]>>(16*(j%2)));int16_t v[3]={(int16_t)dr[0],(int16_t)(dr[0]>>16),(int16_t)dr[1]};for(int r=0;r<3;r++){int64_t x=0;for(int j=0;j<3;j++)x+=int64_t(mat[r*3+j])*v[j];x>>=12;dr[25+r]=(uint)x;dr[9+r]=(uint)std::clamp<int64_t>(x,-32768,32767);}}}
void HeapAlloc(Context&c,Mem&m){static uint p=0x80400000;c.V0=p;p+=(c.A0+15)&~15u;if(p>0x80700000)throw std::runtime_error("oracle heap bound");}
void func_800742A8(Context&,Mem&){throw std::runtime_error("0x2A not part of this oracle");}
void Dispatch(Context&,Mem&,uint p){throw std::runtime_error("Unexpected jump table target "+std::to_string(p));}
void func_8001027C(Context&,Mem&);void func_800811E0(Context&,Mem&);void func_80010614(Context&,Mem&);
std::vector<byte> read(const char*p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing input");return std::vector<byte>(std::istreambuf_iterator<char>(f),{});}
'''
main=r'''
int main(int argc,char**argv){try{if(argc!=4)throw std::runtime_error("usage oracle EXE actor.psx output.bin");auto exe=read(argv[1]),asset=read(argv[2]);Mem m;uint base=*(uint*)&exe[0x18];std::copy(exe.begin()+0x800,exe.end(),m.b.begin()+(base&0x7fffff));uint model=0x80100000,entry=0x800a0904,actor=0x80300000;std::copy(asset.begin(),asset.end(),m.b.begin()+(model&0x7fffff));uint table=model+664,meta=model+m.ReadU32(model+4),anim=0,hier=0;
while(m.ReadU32(meta)!=0xffffffff){uint tag=m.ReadU32(meta),len=m.ReadU32(meta+4);if(tag==0x2c)anim=meta+8;if(tag==0x52454948)hier=meta+8;meta+=8+len;}if(!anim||!hier)throw std::runtime_error("Missing original animation/hierarchy");m.WriteU32(entry+0x10,table);m.WriteU32(entry+0x14,model);m.WriteU32(entry+0x18,anim);m.WriteU32(entry+0x1c,hier);m.WriteU8(actor+0x1b,0);m.WriteU16(actor+0x136,0xffff);m.WriteU16(actor+0x138,0xffff);
uint clips=m.ReadU32(anim);std::ofstream out(argv[3],std::ios::binary);out.write("ANM2",4);out.write((char*)&clips,4);uint total=0;
for(uint clip=0;clip<clips;clip++){uint frames=m.ReadU32(anim+8+clip*8);out.write((char*)&clip,4);out.write((char*)&frames,4);m.WriteU16(actor+0x126,clip);for(uint frame=0;frame<frames;frame++){m.WriteU16(actor+0x124,frame);Context c;c.A0=actor;func_80010614(c,m);uint p=m.offset(c.V0,18*24);out.write((char*)&m.b[p],18*24);total++;}}std::cout<<clips<<" clips, "<<total<<" native frames decoded\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
'''
code=preamble+'\n'.join(extract(n) for n in ['func_8001027C','func_800811E0','func_80010614'])+main
cpp=a.out/'animation_oracle.cpp';cpp.write_text(code)
subprocess.run(['g++','-O2','-std=c++17',str(cpp),'-o',str(a.out/'animation-oracle')],check=True)
