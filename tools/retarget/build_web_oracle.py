#!/usr/bin/env python3
"""Compile the supplied SM1 socket and line-input routines as an OFFLINE oracle.

The final perspective/projection call is a recorder, not a game renderer. This
executes native socket math and the real native line loop, including its scratch
carry between segments, with the shipping retarget core at the hook position.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    source=(a.project/'spiderman/generated/main.cs').read_text();records=[]
    def extract(name):
        start=source.index('    public static void '+name+'(');op=source.index('{',start);depth=1;p=op+1
        while depth:depth+=int(source[p]=='{')-int(source[p]=='}');p+=1
        text=source[start:p];records.append({'function':name,'sourceSha256':hashlib.sha256(text.encode()).hexdigest()})
        text=re.sub(r'public static void (\w+)\(CpuContext c, IMemory m\)',r'void \1(Context& c, Mem& m)',text)
        text=re.sub(r'\s*RecompOne.Runtime.Diagnostics.CallRing.Enter\([^;]+;', '', text)
        text=text.replace('Recompiled.SuitWebAttachment.ProjectSwingSegment(c, m);','webhook(c, m);')
        text=text.replace('var _r','auto _r').replace('int.MinValue','INT32_MIN').replace('SpiderMan.','')
        text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU(8|16)\(c, (\d+), m, (.*), (true|false)\);',lambda m:f'c.r[{m[2]}] = '+(f'(uint)(int)(int{m[1]}_t)' if m[4]=='true' else '')+f'm.ReadU{m[1]}({m[3]});',text)
        text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.LoadU32\(c, (\d+), m, (.*)\);',r'c.r[\1] = m.ReadU32(\2);',text)
        text=re.sub(r'RecompOne.Runtime.Hardware.GteScreen.StoreU32\(m, (.*), (.*), c.GetGteVertexTag\(\d+\)\);',r'm.WriteU32(\1, \2);',text)
        text=text.replace('RecompOne.Runtime.Gte.','Gte::')
        if 'RecompOne.' in text or 'Recompiled.' in text:raise ValueError('Untranslated source construct')
        if name=='ptr_8002CEE4' and 'webhook(c, m);' not in text:raise ValueError('Shipping line hook missing')
        return text
    names=['func_80065D3C','func_80065E64','func_80065AC0','func_8002CDFC','func_8007FA00','func_80073E18','ptr_8002CEE4']
    support=Path(__file__).resolve().parent/'native/web_oracle_support.cpp'
    text=support.read_text();pre,post=text.split('// INSERT_ORIGINAL_ROUTINES_HERE')
    cpp=a.out/'web_oracle.cpp';cpp.write_text(pre+'\n'.join(extract(n) for n in names)+post)
    core=a.project/'tools/retarget/native/retarget_core.cpp'
    subprocess.run(['g++','-O2','-std=c++17','-ffp-contract=off',str(cpp),str(core),'-o',str(a.out/'web-oracle')],check=True)
    (a.out/'native-sources.json').write_text(json.dumps({'kind':'Offline original socket/line-input oracle; projection replaced with a coordinate recorder; NOT gameplay','functions':records,'coreSha256':hashlib.sha256(core.read_bytes()).hexdigest()},indent=2)+'\n')
if __name__=='__main__':main()
