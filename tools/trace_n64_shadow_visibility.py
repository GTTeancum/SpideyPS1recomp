"""Execute USA ROM visibility branches; collision answers are controlled fixtures.

Run trace_n64_shadows.py first. Outputs are numeric observations, never ROM code.
"""
import json, struct, sys, hashlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / '.local/n64-shadow'
sys.path.insert(0, str(BASE/'python'))
from unicorn import *
from unicorn.mips_const import *
rom = (ROOT/'Spider-Man (USA).z64').read_bytes()
assert hashlib.sha256(rom).hexdigest() == 'feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c'
ram = (BASE/'main-ram.bin').read_bytes()
def machine():
    u = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN)
    u.mem_map(0, 0x800000); u.mem_write(0,ram)
    u.reg_write(UC_MIPS_REG_CP0_STATUS,u.reg_read(UC_MIPS_REG_CP0_STATUS)|0x20000000)
    return u
def word(u,a,v): u.mem_write(a&0x7fffff,struct.pack('>I',v))
def floats(u,a,v): u.mem_write(a&0x7fffff,struct.pack('>'+'f'*len(v),*v))
def read(u,a,n): return list(struct.unpack('>'+'f'*n,u.mem_read(a&0x7fffff,n*4)))

def replay(name, **values):
    c = dict(name=name,level=3,demo=False,state=0,attached=False,script_hidden=False,
             valid=True,material=True,material_flags=0,eye=[0,-100,0],player=[0,-100,0],
             height=100,forward_y=0,occluded=False,long_valid=True)
    c.update(values); u=machine(); p=0x80301000; cam=0x80300000; sp=0x803fff00
    word(u,0x800e4144,c['level']);word(u,0x800e414c,14 if c['demo'] else 0)
    u.mem_write(0xe41a0,bytes([4 if c['script_hidden'] else 0]))
    word(u,p+0x1118,c['state']);word(u,p+0xcf0,int(c['attached']))
    floats(u,p+0x24c,c['player']); floats(u,p+0x260,[c['height']])
    for i,v in enumerate(c['player']):word(u,p+4+i*4,int(v*4096)&0xffffffff)
    floats(u,cam+0x90,[v/16 for v in c['eye']]);floats(u,cam+0x84,[c['forward_y']])
    floats(u,cam+0x120,[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])
    record=p+0x121c
    floats(u,record+4,[0,-1,0]);floats(u,record+0x14,[c['height']])
    word(u,record+0x20,int(c['valid']));word(u,record+0x38,0x80305000 if c['material'] else 0)
    word(u,0x80305060,c['material_flags'])
    result=dict(draw=False,mode=0,rays=[])
    def hook(u,pc,size,data):
        if pc in (0x80036730,0x80036734):
            result['mode']=u.reg_read(UC_MIPS_REG_S2);u.emu_stop()
        elif pc == 0x8004a204:
            result['init_float']=u.reg_read(UC_MIPS_REG_A1)
            u.reg_write(UC_MIPS_REG_PC,u.reg_read(UC_MIPS_REG_RA))
        elif pc == 0x8004c0b0:
            q=u.reg_read(UC_MIPS_REG_A0)
            if result.pop('init_float'):
                result['rays'].append(dict(kind='camera',start=read(u,q+0xa4,3),end=read(u,q+0xb0,3)))
                word(u,q+0x68,int(c['occluded']))
            else:
                v=struct.unpack('>6i',u.mem_read(q&0x7fffff,24))
                result['rays'].append(dict(kind='long',start=[x/4096 for x in v[:3]],end=[x/4096 for x in v[3:]]))
                word(u,q+0xf8,record if c['long_valid'] else 0)
            u.reg_write(UC_MIPS_REG_PC,u.reg_read(UC_MIPS_REG_RA))
        elif pc == 0x800ce0d4:
            u.reg_write(UC_MIPS_REG_PC,u.reg_read(UC_MIPS_REG_RA))
        elif pc == 0x80037acc:
            result['draw']=True
            # No RDP operations: return through the real renderer epilogue.
            u.reg_write(UC_MIPS_REG_PC,0x80037df8)
    u.hook_add(UC_HOOK_CODE,hook)
    for r,v in [(UC_MIPS_REG_SP,sp),(UC_MIPS_REG_S1,p),(UC_MIPS_REG_S3,cam)]:u.reg_write(r,v)
    u.emu_start(0x800364ec,0x80010000,timeout=2000000,count=100000)
    assert u.reg_read(UC_MIPS_REG_PC) in (0x80036730,0x80036734)
    return dict(input=c,output=result)

cases=[replay('ordinary'),replay('no receiver',valid=False),replay('untextured',material=False),
       replay('translucent receiver',material_flags=1),replay('other material flag',material_flags=2),
       replay('excluded state',state=0x400),replay('combined state',state=0x401),
       replay('script suppression',script_hidden=True),replay('unrelated attachment',attached=True),
       replay('distance boundary',eye=[1500,-100,0]),replay('distance beyond',eye=[1501,-100,0]),
       replay('camera occlusion',occluded=True),replay('below floor looking down',eye=[0,200,0],forward_y=.6),
       replay('camera angle boundary',eye=[0,200,0],forward_y=.5),
       replay('camera height boundary',eye=[0,100,0],forward_y=.6),
       replay('occluded below floor',eye=[0,200,0],forward_y=.6,occluded=True),
       replay('untextured below floor',eye=[0,200,0],forward_y=.6,material=False)]
for state in [0x80,0x400,0x800000]:cases.append(replay('mode state '+hex(state),state=state))
for level in [0x13,0x15,0x18,0x42]:
    for demo in [False,True]:cases.append(replay(f'level {level} demo {demo}',level=level,demo=demo,attached=True))
cases.append(replay('long probe miss',level=0x42,long_valid=False))

# Execute script handlers themselves, including their original special-case skip.
scripts=[]
for level,script,opcode,operand,hidden in [(2,234,195,1,False),(2,233,195,1,False),
    (1,234,195,1,False),(3,1,195,0,True),(3,366,195,0,True),(2,234,195,1,True),
    (25,349,199,0,True),(25,348,199,0,True)]:
    u=machine();sp=0x803fff00;data=0x80310000
    word(u,0x800e4144,level);word(u,sp+0x9c,script)
    u.mem_write(0xe41a0,bytes([4 if hidden else 0]));u.mem_write(data&0x7fffff,struct.pack('>H',operand))
    u.reg_write(UC_MIPS_REG_SP,sp);u.reg_write(UC_MIPS_REG_S3,data)
    # C7 clears flags before invoking the unrelated player-action decoder.
    end=0x800a73a0 if opcode==199 else 0x800a6800
    def stop(u,pc,size,data):
        if pc==end:u.emu_stop()
    u.hook_add(UC_HOOK_CODE,stop)
    u.emu_start(0x800a736c if opcode==199 else 0x800a68ec,0x80010000,timeout=1000000,count=1000)
    assert u.reg_read(UC_MIPS_REG_PC)==end,(level,script,opcode,hex(u.reg_read(UC_MIPS_REG_PC)))
    scripts.append(dict(level=level,script=script,opcode=opcode,operand=operand,before=hidden,
                        after=bool(u.mem_read(0xe41a0,1)[0]&4)))

levels=[]
for base,count,first in [(0xe4e50,34,0),(0xe4d84,17,50)]:
    for i in range(count):
        p=struct.unpack_from('>I',ram,base+12*i+4)[0]&0x7fffff
        levels.append(dict(index=first+i,name=ram[p:ram.index(b'\0',p)].decode()))
guards=[]
for lane in range(4):
    for value in [-32701,-32700,32700,32701]:
        u=machine();sp=0x803fff00;row=[0,0,0,1];row[lane]=value
        floats(u,sp+0x100,row);u.reg_write(UC_MIPS_REG_SP,sp)
        def stop_guard(u,pc,size,data):
            if pc in (0x80037acc,0x80037df8):u.emu_stop()
        u.hook_add(UC_HOOK_CODE,stop_guard)
        u.emu_start(0x80037a20,0x80010000,timeout=1000000,count=200)
        pc=u.reg_read(UC_MIPS_REG_PC);assert pc in (0x80037acc,0x80037df8)
        guards.append(dict(row=row,accepted=pc==0x80037acc))
out=dict(cases=cases,scripts=scripts,levels=levels,fixed_matrix_guard=guards)
(BASE/'visibility-oracle.json').write_text(json.dumps(out,indent=2))
# Check bindings against both actual programs, rather than trusting mirrored
# constants in a C# test. The paired physics loads precede matching branches.
raw=(ROOT/'spiderman/extracted/SLUS_008.75').read_bytes()
ps1_base=struct.unpack_from('<I',raw,0x18)[0]-0x800
bindings=[]
for label,n64,ps1,n_offset,p_offset in [('state',0x800842a4,0x80035864,0x1118,0xf3c),
                                      ('attached',0x80084238,0x800357f4,0xcf0,0xbc4)]:
    n=struct.unpack_from('>I',ram,n64&0x7fffff)[0]
    p=struct.unpack_from('<I',raw,ps1-ps1_base)[0]
    assert n>>26==p>>26==0x23 and n&65535==n_offset and p&65535==p_offset,(label,hex(n),hex(p))
    bindings.append(dict(field=label,n64_pc=hex(n64),ps1_pc=hex(ps1),n64_offset=n_offset,ps1_offset=p_offset))
for opcode,n_handler,p_handler in [(195,0x800a68ec,0x8005c984),(199,0x800a736c,0x8005d06c)]:
    assert struct.unpack_from('>I',ram,0x21378+(opcode-2)*4)[0]==n_handler
    assert struct.unpack_from('<I',raw,0x80094ad4-ps1_base+(opcode-2)*4)[0]==p_handler
sys.path.insert(0,str(ROOT/'spiderman/tools'))
from cdwad import Wad
archive=Wad();triggers=[]
for name,index,opcode in [('l1a3_t.trg',234,195),('l1a4_t.trg',366,195),('l7a3_t.trg',349,199)]:
    data=archive.read(name);offset=struct.unpack_from('<I',data,12+index*4)[0]
    assert struct.unpack_from('<H',data,offset)[0]==6
    assert struct.unpack_from('<H',data,offset+8)[0]==opcode
    triggers.append(dict(level=name,script=index,first_opcode=opcode))
(BASE/'ps1-bindings.json').write_text(json.dumps(dict(fields=bindings,triggers=triggers),indent=2))
print(f'Executed {len(cases)} ROM draw paths and {len(scripts)} script paths; decoded {len(levels)} level mappings.')
