#!/usr/bin/env python3
"""Executed Python/asset/installer checks; managed C# execution is a separate suite."""
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import struct
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools'),str(Path(__file__).resolve().parent),str(ROOT/'tools/retarget')]
from asset_safety.native_actor import inspect_actor_lods
import install_dreamcast_sfd as sfd
from build_native import commands_for
from build_linux import verify_elf

def fixture():
    b=bytearray(128)
    for offset,value in [(0,4),(4,116),(8,1),(48,2),(52,60),(56,88),(116,0x2a),(120,0),(124,0xffffffff)]:
        struct.pack_into('<I',b,offset,value)
    struct.pack_into('<H',b,86,1);struct.pack_into('<H',b,114,0xffff)
    return b

def both(value):return struct.pack('<I',value)+struct.pack('>I',value)

def record(name,extent,size,is_dir=False):
    if isinstance(name,str):name=name.encode('ascii')
    length=33+len(name);length+=length%2
    b=bytearray(length);b[0]=length;b[2:10]=both(extent);b[10:18]=both(size);b[25]=2 if is_dir else 0
    b[28:32]=b'\x01\0\0\x01';b[32]=len(name);b[33:33+len(name)]=name
    return bytes(b)

def header(lba):
    frames=lba+150;minute,frames=divmod(frames,4500);second,frame=divmod(frames,75)
    def enc(n):return (n//10)*16+n%10
    return sfd.SYNC+bytes([enc(minute),enc(second),enc(frame),1])

def fake_disc(root,pregap=225):
    base=45000;sectors=[bytearray(2048) for _ in range(64)]
    names=[n+'.SFD' for n in sfd.STORY]+['ATVILOGO.SFD'];expected={}
    entries=[record(b'\0',base+20,2048,True),record(b'\1',base+20,2048,True)]
    for i,name in enumerate(names):
        data=b'\0\0\x01\xba'+name.encode();sectors[21+i][:len(data)]=data;expected[name]=data
        entries.append(record(name+';1',base+21+i,len(data)))
    listing=b''.join(entries);assert len(listing)<2048;sectors[20][:len(listing)]=listing
    pvd=sectors[16];pvd[:7]=b'\x01CD001\x01';pvd[40:72]=b'SMDC1'.ljust(32,b' ');pvd[80:88]=both(64);pvd[128:132]=b'\0\x08\x08\0';pvd[156:190]=record(b'\0',base+20,2048,True)
    raw=b'\0'*(pregap*2352)+b''.join(header(base+i)+bytes(sector)+b'\0'*(2352-2064) for i,sector in enumerate(sectors))
    track=root/'track.bin';track.write_bytes(raw)
    cue=root/'fixture.cue';cue.write_text('FILE "track.bin" BINARY\n TRACK 03 MODE1/2352\n INDEX 00 00:00:00\n INDEX 01 00:03:00\n')
    gdi=root/'fixture.gdi';gdi.write_text(f'1\n3 {base} 4 2352 "track.bin" {pregap*2352}\n')
    return cue,gdi,track,expected

class LodTests(unittest.TestCase):
    def test_valid_chain_and_no_modification(self):
        b=fixture();before=bytes(b);v=inspect_actor_lods(b);self.assertEqual((v.objects,v.meshes,v.terminal_lods),(1,2,1));self.assertEqual(bytes(b),before)
    def test_extra_terminal(self):
        b=fixture();struct.pack_into('<H',b,86,0xffff)
        with self.assertRaisesRegex(ValueError,'terminal LODs'):inspect_actor_lods(b)
    def test_all_truncations(self):
        b=fixture()
        for i in range(len(b)):
            with self.subTest(length=i),self.assertRaises(ValueError):inspect_actor_lods(b[:i])
    def test_bad_offsets_and_counts(self):
        for pos in (4,8,48,52,56,120):
            b=fixture();struct.pack_into('<I',b,pos,0xffffffff)
            with self.subTest(offset=pos),self.assertRaises(ValueError):inspect_actor_lods(b)
    def test_mutations_are_bounded(self):
        rng=random.Random(50406)
        for i in range(10000):
            b=fixture()
            for _ in range(1+i%5):b[rng.randrange(len(b))]^=rng.randrange(1,256)
            try:inspect_actor_lods(b)
            except ValueError:pass
    def test_texture_only(self):self.assertFalse(inspect_actor_lods(bytes(12)).animated)
    def test_every_bundled_actor(self):
        count=0
        for game in ('spiderman','spiderman2'):
            with zipfile.ZipFile(ROOT/game/'port/bundled/runtime-assets.zip') as z:
                for name in z.namelist():
                    if '/' not in name and name.lower().endswith('.psx'):
                        b=z.read(name);before=hashlib.sha256(b).digest();inspect_actor_lods(b,name);self.assertEqual(hashlib.sha256(b).digest(),before);count+=1
        self.assertEqual(count,121)
    def test_corrected_sewer_lizards(self):
        with zipfile.ZipFile(ROOT/'spiderman/port/bundled/runtime-assets.zip') as z:
            for name in ('lizman.psx','lizman2.psx'):
                a=inspect_actor_lods(z.read(name),name);self.assertEqual((a.objects,a.meshes,a.terminal_lods),(19,38,19))
    def test_preserved_rig_suits(self):
        for suit in ('smu-spiderham','smu-2099'):
            b=(ROOT/'spiderman/port/mods/suits'/suit/'actor.psx').read_bytes();a=inspect_actor_lods(b,suit)
            self.assertEqual((a.objects,a.meshes,a.terminal_lods),(18,18,18))

class DiscTests(unittest.TestCase):
    def test_extended_minute_headers(self):
        for lba in (0,45000,159551,449849,449850,549149):self.assertEqual(sfd.raw_lba(header(lba)),lba)
    def test_invalid_raw_header(self):
        for b in (bytes(16),header(45000)[:15],sfd.SYNC+b'\x10\x6a\0\1',sfd.SYNC+b'\x10\x60\0\1',sfd.SYNC+b'\x10\0\x75\1'):
            with self.assertRaises(ValueError):sfd.raw_lba(b)
    def test_cue_gdi_pregap_and_install(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root);before=hashlib.sha256(track.read_bytes()).digest()
            a=sfd.parse_disc(cue);b=sfd.parse_disc(gdi);self.assertEqual(a,b);self.assertEqual(a[0].byte_offset,529200)
            for descriptor in (cue,gdi):
                out=root/(descriptor.suffix[1:]+'-out');j=sfd.install(descriptor,out)
                self.assertEqual(len(j['movies']),22);self.assertFalse(j['playback_implemented_by_this_tool'])
                for name,data in expected.items():self.assertEqual((out/name).read_bytes(),data)
                self.assertEqual(hashlib.sha256(track.read_bytes()).digest(),before)
    def test_original_fallback_files_untouched_and_existing_output_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root);out=root/'installed';out.mkdir();sentinel=out/'L1M1.STR';sentinel.write_bytes(b'original PS1')
            with self.assertRaises(ValueError):sfd.install(cue,out)
            self.assertEqual(sentinel.read_bytes(),b'original PS1')
    def test_sector_corruption_is_atomic(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root);b=bytearray(track.read_bytes());b[(225+40)*2352+12]=0xff;track.write_bytes(b)
            out=root/'installed'
            with self.assertRaises(ValueError):sfd.install(cue,out)
            self.assertFalse(out.exists());self.assertFalse(list(root.glob('.sfd-stage-*')))
    def test_audio_gaps_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root)
            with sfd.Disc(sfd.parse_disc(cue)) as disc:
                with self.assertRaises(ValueError):disc.read(44999,4096)
    def test_path_escape_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            for name in ('../outside.bin','/absolute.bin','C:\\outside.bin'):
                with self.assertRaises((ValueError,FileNotFoundError)):sfd.safe_input(root,name)
    def test_unsupported_cue_layout_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root);cue.write_text(cue.read_text().replace('MODE1/2352','MODE1/2048'))
            with self.assertRaisesRegex(ValueError,'authoritative absolute LBAs'):sfd.parse_disc(cue)
    def test_gdi_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);cue,gdi,track,expected=fake_disc(root);gdi.write_text(gdi.read_text().replace('45000','45001'))
            with self.assertRaisesRegex(ValueError,'disagrees'):sfd.parse_disc(gdi)
    def test_endian_mismatch_rejected(self):
        with self.assertRaisesRegex(ValueError,'endian'):sfd.both32(struct.pack('<II',1,2),0)

class BuildStaticTests(unittest.TestCase):
    def test_freestanding_linux_commands_reject_external_symbols(self):
        command=commands_for('linux-x64',Path('/temporary'),freestanding_linux=True)[0]
        self.assertIn('-nostdlib',command);self.assertIn('-fuse-ld=lld',command)
        self.assertIn('-Wl,--no-undefined',command)
    def test_linux_native_commands_do_not_require_windows_linker(self):
        cmds=commands_for('linux-x64',Path('/temporary'));self.assertEqual(len(cmds),1);self.assertIn('--target=x86_64-unknown-linux-gnu',cmds[0]);self.assertFalse(any('lld-link' in c for c in cmds))
    def test_windows_native_commands_do_not_build_elf(self):
        cmds=commands_for('win-x64',Path('/temporary'));self.assertEqual(len(cmds),2);self.assertFalse(any('-fPIC' in c for c in cmds))
    def test_target_rid_selection_static(self):
        shared=ET.parse(ROOT/'tools/RecompOne/native/Runtime.props').getroot()
        self.assertIn("'$(OpenSpideyNativeRid)' == 'win-x64'",shared.find('Import').attrib['Condition'])
        self.assertIn("'$(OpenSpideyNativeRid)' == 'linux-x64'",shared.find('ItemGroup').attrib['Condition'])
        for game,assembly in [('spiderman','SpiderMan'),('spiderman2','SpiderMan2')]:
            p=ET.parse(ROOT/game/'port'/(assembly+'.csproj')).getroot()
            for node in p.findall('.//RuntimeIdentifier'):self.assertIn("'$(RuntimeIdentifier)' == ''",node.attrib['Condition'])
            self.assertEqual(p.find('Import').attrib['Project'],'../../tools/RecompOne/native/Runtime.props')
            self.assertEqual(p.find('.//PublishReadyToRun').text,'true')
    def test_correct_game_scripts_static(self):
        for game,number in [('spiderman','1'),('spiderman2','2')]:
            s=(ROOT/game/'tools/build.sh').read_text();self.assertIn('--game '+number,s);self.assertNotIn('xmen',s.lower());self.assertIn('set -euo pipefail',s)
    def test_path_users_share_resolution_static(self):
        for game in ('spiderman','spiderman2'):
            main=(ROOT/game/'port/Program.cs').read_text();mods=(ROOT/game/'patches/SuitMods.cs').read_text()
            self.assertLess(main.index('PrepareLaunchArguments'),main.index('SetCurrentDirectory'))
            self.assertIn('RuntimePaths.ApplicationDirectory',mods);self.assertNotIn('Environment.ProcessPath',mods)
    def test_guard_before_pending_bytes_static(self):
        s=(ROOT/'tools/RecompOne/RecompOne.Runtime/Assets/LooseWadOverrides.cs').read_text()
        self.assertLess(s.index('NativeActorLodValidator.Validate(data'),s.index('_pending = data'))
        self.assertIn('using retail {name}',s)
        start=s.index('public static void FindModel');t=s[start:s.index('public static void FindExit',start)]
        self.assertLess(t.index('_pending = null'),t.index('NativeActorLodValidator.Validate'))
    def test_core_provenance_and_elf(self):
        meta=json.loads((ROOT/'tools/retarget/native/bin/build-provenance.json').read_text())
        for item in meta['files']:self.assertEqual(hashlib.sha256((ROOT/item['path']).read_bytes()).hexdigest(),item['sha256'])
        verify_elf(ROOT/'tools/retarget/native/bin/libOpenSpideyRetarget.so')

if __name__=='__main__':unittest.main(verbosity=2)
