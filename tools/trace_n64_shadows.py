"""Replay the private USA N64 ROM's decompressor and shadow matrix routine.

Run from the repository after sourcing .local/environment.ps1. Install Unicorn
into .local/n64-shadow/python; the ROM and every generated artifact remain private
under this repository. No N64 machine code is included in this tool or its fixture.
The matrix fixture is the actual COP1 result at 80037A20, with an identity camera.
"""
import sys,struct,json,hashlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'.local/n64-shadow'
BASE.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(BASE/'python'))
from unicorn import *
from unicorn.mips_const import *
rom=(ROOT/'Spider-Man (USA).z64').read_bytes()
assert hashlib.sha256(rom).hexdigest() == 'feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c', 'Expected USA revision 0 ROM'
u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN);u.mem_map(0,0x800000)
u.mem_write(0x400,rom[0x1000:0x13b30]); table=0x13b34;n=struct.unpack_from('>I',rom,table)[0]; records=[]
for i in range(n):
 start,end=struct.unpack_from('>II',rom,table+4+4*i);block=rom[table+start:table+end];kind=block[3];size,packed=struct.unpack_from('>II',block,4)
 assert block[:3]==b'ERZ' and size<=0x10000,(i,block[:12])
 u.mem_write(0x3e0000,block);dest=0x16ae0+i*0x10000
 for reg,val in [(UC_MIPS_REG_SP,0x803dfff0),(UC_MIPS_REG_RA,0x80010000),(UC_MIPS_REG_A0,0x803e0000),(UC_MIPS_REG_A1,0x80000000+dest),(UC_MIPS_REG_A2,packed),(UC_MIPS_REG_A3,size)]:u.reg_write(reg,val)
 try:u.emu_start(0x80000c50 if kind==2 else 0x80001340,0x80010000,timeout=2000000,count=20000000)
 except UcError as e: print('ERROR',e,hex(u.reg_read(UC_MIPS_REG_PC)));raise
 assert u.reg_read(UC_MIPS_REG_PC)==0x80010000
 data=bytes(u.mem_read(dest,size)); records.append(dict(index=i,rom=hex(table+start),ram=hex(0x80000000+dest),size=size,compressed=packed,kind=kind,sha256=hashlib.sha256(data).hexdigest()))
 print(records[-1])
ram=bytes(u.mem_read(0,0x200000));(BASE/'main-ram.bin').write_bytes(ram);(BASE/'blocks.json').write_text(json.dumps(records,indent=2))

ram=(BASE/'main-ram.bin').read_bytes()
cases=[]
for normal in [(0,-1,0),(.6,-.8,0),(0,-.8,.6),(.36,-.8,.48),(0,1,0),(1,0,0)]:
 u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN);u.mem_map(0,0x800000);u.mem_write(0,ram)
 def floats(addr, vals): u.mem_write(addr&0x7fffff,struct.pack('>'+'f'*len(vals),*vals))
 def word(addr,val):u.mem_write(addr&0x7fffff,struct.pack('>I',val))
 sp=0x803fff00;camera=0x80300000;player=0x80301000;nptr=0x80304000
 floats(camera+0x120,[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])
 floats(player+0x24c,[1600,-400,3200]);floats(nptr,normal)
 word(sp+16,nptr);word(sp+20,1)
 for reg,val in [(UC_MIPS_REG_SP,sp),(UC_MIPS_REG_RA,0x80010000),(UC_MIPS_REG_A0,0x80305000),(UC_MIPS_REG_A1,camera),(UC_MIPS_REG_A2,player),(UC_MIPS_REG_A3,struct.unpack('>I',struct.pack('>f',800))[0])]:u.reg_write(reg,val)
 u.reg_write(UC_MIPS_REG_CP0_STATUS,u.reg_read(UC_MIPS_REG_CP0_STATUS)|0x20000000)
 try:u.emu_start(0x800378b0,0x80037a20,timeout=2000000,count=100000)
 except UcError as e:print('ERROR',e,hex(u.reg_read(UC_MIPS_REG_PC)));raise
 assert u.reg_read(UC_MIPS_REG_PC)==0x80037a20
 matrix=list(struct.unpack('>16f',u.mem_read((sp-0x138+0xd0)&0x7fffff,64)))
 case=dict(normal=normal,anchor=[100,50,200],matrix=matrix);cases.append(case);print(case)
(BASE/'matrix-oracle.json').write_text(json.dumps(cases,indent=2))

# Replay the common receiver selector too. In particular, branch delay slots
# matter here: a non-null receiver object WITH bit 0 set suppresses the shadow.
receivers=[]
for valid,object_address,flags in [(0,0,0),(1,0,0),(1,0x80305000,0),(1,0x80305000,1)]:
 u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN);u.mem_map(0,0x800000);u.mem_write(0,ram)
 def word(addr,val):u.mem_write(addr&0x7fffff,struct.pack('>I',val))
 def floats(addr,vals):u.mem_write(addr&0x7fffff,struct.pack('>'+'f'*len(vals),*vals))
 player=0x80301000;record=player+0x121c
 word(0x800e4144,3);word(0x800e414c,0)
 floats(record+4,[0,-1,0]);floats(record+0x14,[800])
 word(record+0x20,valid);word(record+0x38,object_address)
 if object_address:word(object_address+0x60,flags)
 for reg,val in [(UC_MIPS_REG_SP,0x803fff00),(UC_MIPS_REG_RA,0x80010000),(UC_MIPS_REG_A0,player)]:u.reg_write(reg,val)
 u.reg_write(UC_MIPS_REG_CP0_STATUS,u.reg_read(UC_MIPS_REG_CP0_STATUS)|0x20000000)
 u.emu_start(0x8003632c,0x80010000,timeout=2000000,count=100000)
 assert u.reg_read(UC_MIPS_REG_PC)==0x80010000
 result=u.reg_read(UC_MIPS_REG_V0)
 expected=0 if not valid or flags&1 else 1 if object_address else 2
 assert result==expected
 case=dict(valid=valid,object_present=bool(object_address),object_flags=flags,result=result,
           receiver=list(struct.unpack('>4f',u.mem_read((player+0x260)&0x7fffff,16))))
 receivers.append(case);print(case)
(BASE/'receiver-oracle.json').write_text(json.dumps(receivers,indent=2))
