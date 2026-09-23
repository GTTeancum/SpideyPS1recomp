#!/usr/bin/env python3
"""Execute two original native animation routines in a bounded frame-zero fixture.

This is a native memory-corruption regression, NOT gameplay. No GTE instruction
is approximated: the fixture throws if it reaches one. Old broken actor files
come from the evidence package; corrected actors come from the current bundle.
"""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, zipfile
def extract(s, name):
 start=s.index('    public static void '+name+'(');op=s.index('{',start);depth=1;p=op+1
 while depth:depth+=int(s[p]=='{')-int(s[p]=='}');p+=1
 text=s[start:p]
 text=re.sub(r'public static void (\w+)\(CpuContext c, IMemory m\)',r'void \1(Context& c, Mem& m)',text)
 text=re.sub(r'\s*RecompOne.Runtime.Diagnostics.CallRing.Enter\([^;]+;', '', text)
 text=text.replace('var _r','auto _r').replace('int.MinValue','INT32_MIN').replace('SpiderMan.','')
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU(8|16)\(c, (\d+), m, (.*), (true|false)\);',lambda m:f'c.r[{m[2]}] = '+(f'(uint)(int)(int{m[1]}_t)' if m[4]=='true' else '')+f'm.ReadU{m[1]}({m[3]});',text)
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU32\(c, (\d+), m, (.*)\);',r'c.r[\1] = m.ReadU32(\2);',text)
 text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.StoreU32\(m, (.*), (.*), c.GetGteVertexTag\(\d+\)\);',r'm.WriteU32(\1, \2);',text)
 text=text.replace('RecompOne.Runtime.Gte.','Gte::')
 if 'RecompOne.' in text:raise ValueError('unsupported construct')
 return text

PREAMBLE = '\n#include <cstdint>\n#include <vector>\n#include <fstream>\n#include <iostream>\n#include <stdexcept>\n#include <cstring>\n#include <algorithm>\nusing uint=uint32_t; using ushort=uint16_t; using byte=uint8_t;\nconstexpr uint Scratch=0x000A58E4, Collision=0x000A5BB4;\nstruct Context { union { uint r[32];struct {uint Zero,At,V0,V1,A0,A1,A2,A3,T0,T1,T2,T3,T4,T5,T6,T7,S0,S1,S2,S3,S4,S5,S6,S7,T8,T9,K0,K1,GP,SP,FP,RA;};};uint LO=0,HI=0;Context(){for(auto&x:r)x=0;SP=0x807ff000;}template<class...T>void SetDerived(int i,uint v,T...){r[i]=v;}};\nstruct Mem {\n std::vector<byte>b=std::vector<byte>(0x800000), touched=std::vector<byte>(0x1000); bool track=false;\n uint offset(uint p,uint n){uint q=p&0x1fffffff;if(q+n>b.size())throw std::runtime_error("RAM bounds");return q;}\n uint ReadU8(uint p){return b[offset(p,1)];}uint ReadU16(uint p){uint q=offset(p,2);return b[q]|(b[q+1]<<8);}uint ReadU32(uint p){return ReadU16(p)|(ReadU16(p+2)<<16);}\n void WriteU8(uint p,byte v){uint q=offset(p,1); b[q]=v; if(track && q>=Scratch && q<Scratch+touched.size())touched[q-Scratch]=1;}\n void WriteU16(uint p,ushort v){WriteU8(p,v);WriteU8(p+1,v>>8);}void WriteU32(uint p,uint v){WriteU16(p,v);WriteU16(p+2,v>>16);}\n uint ReadWordLeft(uint v,uint p){unsigned shift=(3-(p&3))*8;uint mask=shift?((1u<<shift)-1):0;return (v&mask)|(ReadU32(p&~3u)<<shift);}\n uint ReadWordRight(uint v,uint p){unsigned shift=(p&3)*8;return (v&~(0xffffffffu>>shift))|(ReadU32(p&~3u)>>shift);}\n void WriteWordLeft(uint p,uint v){uint low=p&~3u,k=p&3;for(uint i=0;i<=k;i++)WriteU8(low+i,v>>((3-k+i)*8));}\n void WriteWordRight(uint p,uint v){uint low=p&~3u,k=p&3;for(uint i=k;i<4;i++)WriteU8(low+i,v>>((i-k)*8));}\n};\nnamespace Gte {void Write(int,uint){}void ReadTo(Context&,int,int){}void Execute(uint){throw std::runtime_error("Interpolation is outside frame-zero copy fixture; no GTE operation was approximated");}}\nvoid func_80074310(Context&,Mem&);\nstd::vector<byte> read(const char*p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing input");return std::vector<byte>(std::istreambuf_iterator<char>(f),{});}\n'
ENTRYPOINT = '\nint main(int argc,char**argv){try{\n if(argc!=2)throw std::runtime_error("usage sewer-oracle actor.psx"); auto a=read(argv[1]);Mem m; uint model=0x80100000,entry=0x800A0904,actor=0x80300000;\n if(a.size()>0x100000)throw std::runtime_error("asset bound"); std::copy(a.begin(),a.end(),m.b.begin()+0x100000);\n uint n=m.ReadU32(model+8),table=model+12+n*36, meshes=m.ReadU32(table),terminals=0;\n if(meshes>10000)throw std::runtime_error("mesh bound");for(uint i=0;i<meshes;i++)if(m.ReadU16(model+m.ReadU32(table+4+i*4)+26)==0xffff)terminals++;\n uint meta=model+m.ReadU32(model+4),anim=0;for(uint i=0;i<4096;i++){uint tag=m.ReadU32(meta);if(tag==0xffffffff)break;if(tag==0x2a)anim=meta+8;meta+=8+m.ReadU32(meta+4);}if(!anim)throw std::runtime_error("not a native 0x2A animation bank");\n m.WriteU32(entry+0x18,anim);m.WriteU16(entry+0x34,terminals);m.WriteU8(actor+0x1b,0);m.WriteU16(actor+0x126,0);m.WriteU16(actor+0x124,0);\n std::fill(m.b.begin()+Scratch,m.b.begin()+Scratch+0x1000,0xA5);\n m.track=true;Context c;c.A0=actor;func_800742A8(c,m);m.track=false;\n uint written=0,overrun=0,changed=0;for(uint i=0;i<m.touched.size();i++){if(m.touched[i]){written++;if(Scratch+i>=Collision)overrun++;}if(Scratch+i>=Collision && m.b[Scratch+i]!=0xA5)changed++;}\n if(written!=terminals*24)throw std::runtime_error("native write count was not terminal-count times 24");\n std::cout<<"{\\"objects\\":"<<n<<",\\"meshes\\":"<<meshes<<",\\"terminal_lods\\":"<<terminals<<",\\"native_bytes_written\\":"<<written<<",\\"collision_range_bytes_written\\":"<<overrun<<",\\"collision_canary_bytes_changed\\":"<<changed<<",\\"frame\\":0,\\"clip\\":0,\\"gameplay\\":false}\\n";\n return 0;\n}catch(const std::exception&e){std::cerr<<e.what()<<"\\n";return 1;}}\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--fixtures', type=Path, required=True, help='evidence/sewer-native directory from Repair04')
    parser.add_argument('--out', type=Path, required=True, help='new output directory')
    args = parser.parse_args()
    if args.out.exists(): parser.error('output already exists; source and earlier evidence are never overwritten')
    compiler = shutil.which('g++')
    if not compiler: parser.error('g++ is required for this Linux/native proof utility')
    source = args.project / 'spiderman/generated/main.cs'
    s = source.read_text()
    args.out.mkdir(parents=True)
    out = args.out.resolve()
    code = PREAMBLE + extract(s, 'func_80074310') + extract(s, 'func_800742A8') + ENTRYPOINT
    cpp = out / 'sewer_oracle.cpp'; cpp.write_text(code)
    binary = out / 'sewer-oracle'
    command = [compiler, '-O2', '-std=c++17', '-fno-strict-aliasing', str(cpp), '-o', str(binary)]
    subprocess.run(command, check=True)
    results = []
    with zipfile.ZipFile(args.project / 'spiderman/port/bundled/runtime-assets.zip') as bundle:
        for label in ('old-committed', 'corrected-upload'):
            for name in ('lizman.psx', 'lizman2.psx'):
                asset = ((args.fixtures / (label + '-' + name)).read_bytes()
                         if label == 'old-committed' else bundle.read(name))
                fixture = out / (label + '-' + name); fixture.write_bytes(asset)
                result = json.loads(subprocess.check_output([str(binary), str(fixture)], text=True))
                result.update(label=label, asset=name, sha256=hashlib.sha256(asset).hexdigest())
                expected = 168 if label == 'old-committed' else 0
                if result['collision_range_bytes_written'] != expected:
                    raise RuntimeError('unexpected native collision-memory write count: ' + repr(result))
                if (result['collision_canary_bytes_changed'] > 0) != (label == 'old-committed'):
                    raise RuntimeError('unexpected native collision canary change: ' + repr(result))
                results.append(result)
    record = {'boundary': 'Original native routines compiled in a controlled frame-zero fixture; not a game process. No GTE operation executed or approximated.',
              'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'cpp_sha256': hashlib.sha256(cpp.read_bytes()).hexdigest(), 'compile_command': command, 'results': results}
    (out / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(record, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
