#!/usr/bin/env python3
"""Executed native-C-ABI tests. Managed hook assertions are explicitly static.
This is not a test of a running .NET game. Uses the authored tiny.sfd fixture.
"""
import ctypes as C,hashlib,os,shutil,subprocess,tempfile,unittest
from pathlib import Path
from sfd_native import Decoder,load,Info,ReadAt
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
class SfdTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.dir=Path(self.temp.name);self.path=self.dir/'tiny.sfd';self.good=(HERE/'test-fixtures/tiny.sfd').read_bytes();self.path.write_bytes(self.good)
 def tearDown(self):self.temp.cleanup()
 def decode(self,**kwargs):
  with Decoder(self.path,**kwargs) as d:
   h=[hashlib.sha256(),hashlib.sha256()]
   while True:
    b=d.video()
    if b is None:break
    h[0].update(b)
   while True:
    b=d.audio()
    if b is None:break
    h[1].update(b)
   return d.video_count,d.audio_count,[x.hexdigest() for x in h]
 def test_all_frames_samples_and_no_writes(self):
  n,a,_=self.decode();self.assertEqual((n,a),(60,88200));self.assertEqual(self.path.read_bytes(),self.good);self.assertEqual(list(self.dir.iterdir()),[self.path])
 def test_seventeen_byte_callback_reads(self):self.assertEqual(self.decode(),self.decode(max_read=17))
 def test_interleaved_streams_do_not_drop_reference_frames(self):
  reference=self.decode()
  with Decoder(self.path) as d:
   hashes=[hashlib.sha256(),hashlib.sha256()];v=True;a=True
   while v or a:
    if v:
     b=d.video();v=b is not None
     if v:hashes[0].update(b)
    if a:
     b=d.audio(31);a=b is not None
     if a:hashes[1].update(b)
   self.assertEqual((d.video_count,d.audio_count,[x.hexdigest() for x in hashes]),reference)
 def test_audio_chunk_edges_and_exact_tail(self):
  with Decoder(self.path) as d:
   i=0;total=0
   while True:
    b=d.audio([1,31,32,33,2047,2048][i%6]);i+=1
    if b is None:break
    self.assertEqual(set(b),{0});total+=len(b)//4
   self.assertEqual(total,88200);self.assertIsNone(d.audio())
 def test_eof_is_stable(self):
  with Decoder(self.path) as d:
   for _ in range(60):self.assertIsNotNone(d.video())
   self.assertIsNone(d.video());self.assertIsNone(d.video())
 def test_output_capacities_do_not_write(self):
  with Decoder(self.path) as d:
   guard=C.create_string_buffer(b'Z'*100);index=C.c_uint32(123)
   self.assertLess(d.lib.sfd_video(d.ptr,guard,100,C.byref(index)),0);self.assertEqual(guard.raw,b'Z'*100+b'\0');self.assertEqual(index.value,123)
   self.assertIsNotNone(d.video())
 def test_source_io_failure(self):
  with self.assertRaises(ValueError):self.decode(fail_at=32768)
 def test_truncate_held_source(self):
  with Decoder(self.path) as d:
   with self.path.open('r+b') as f:f.truncate(2048)
   with self.assertRaises(ValueError):
    while d.audio() is not None:pass
 def test_held_handle_not_reopened(self):
  if os.name=='nt':self.skipTest('Windows disallows rename of a held read handle')
  reference=self.decode()
  with Decoder(self.path) as d:
   self.path.rename(self.dir/'old.sfd');self.path.write_bytes(b'not a replacement movie')
   hs=[hashlib.sha256(),hashlib.sha256()]
   while True:
    b=d.video()
    if b is None:break
    hs[0].update(b)
   while True:
    b=d.audio()
    if b is None:break
    hs[1].update(b)
   self.assertEqual((d.video_count,d.audio_count,[h.hexdigest() for h in hs]),reference)
 def test_invalid_headers_and_dimensions(self):
  at=self.good.index(b'\0\0\1\xb3')+4;adx=self.good.index(b'\x80\0\0\x1c')
  mutations=[(0,b'Z'),(4,b'\x44'),(adx+19,b'\1'),(adx+4,b'\2'),(at,((721<<12)|32).to_bytes(3,'big')),(adx+12,(0xffffffff).to_bytes(4,'big'))]
  for offset,new in mutations:
   b=bytearray(self.good);b[offset:offset+len(new)]=new;self.path.write_bytes(b)
   with self.subTest(offset=offset),self.assertRaises(ValueError):Decoder(self.path)
 def test_bad_adx_terminator_is_late_error(self):
  b=bytearray(self.good);i=b.rfind(b'\x80\1\0\0');self.assertGreater(i,0);b[i+1]=2;self.path.write_bytes(b)
  with Decoder(self.path) as d:
   self.assertIsNotNone(d.video())
   with self.assertRaises(ValueError):
    while d.audio() is not None:pass
 def test_truncated_packets(self):
  for length in [0,2047,4096,len(self.good)-3,len(self.good)-20]:
   self.path.write_bytes(self.good[:length])
   with self.subTest(length=length),self.assertRaises(ValueError):self.decode()
 def test_rgba_boundaries(self):
  lib=load();yuv=C.create_string_buffer(bytes([16,235,16,235,128,128]));out=C.create_string_buffer(16)
  self.assertEqual(lib.sfd_rgba(yuv,6,2,2,out,16),0);self.assertEqual(out.raw[:8],bytes([0,0,0,255,255,255,255,255]))
  self.assertLess(lib.sfd_rgba(yuv,6,3,2,out,16),0);self.assertLess(lib.sfd_rgba(yuv,6,2,2,out,15),0)
 def test_fixed_workspace(self):
  self.assertLess(load().sfd_workspace_size(),2*1024*1024)
 def test_no_native_library_imports(self):
  if os.name!='nt' and shutil.which('readelf'):
   report=subprocess.check_output(['readelf','-d',str(HERE/'native/bin/libOpenSpideySfd.so')],text=True);self.assertNotIn('NEEDED',report)
 def test_vlc_tables_reproducible(self):
  import importlib.util
  spec=importlib.util.spec_from_file_location('tables',HERE/'native/make_vlc_tables.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  self.assertEqual((HERE/'native/mpeg1_vlc.h').read_text(),m.generate())
 def test_static_production_routes_only_original_sfd(self):
  resolver=(ROOT/'tools/RecompOne/RecompOne.Runtime/Media/MovieResolver.cs').read_text();player=(ROOT/'tools/RecompOne/RecompOne.Runtime/Media/DreamcastMovies.cs').read_text();native=(ROOT/'tools/RecompOne/RecompOne.Runtime/Media/NativeSfd.cs').read_text()
  self.assertIn('stem+".SFD"',resolver);self.assertNotIn('.osmv',resolver);self.assertIn('new NativeSfd(path)',player)
  self.assertNotIn('new MovieCache',player);self.assertNotIn('Process.Start',native+player);self.assertIn('RandomAccess.Read',native);self.assertIn('FileAccess.Read',native)
 def test_static_failure_and_cleanup_hooks_preserved(self):
  player=(ROOT/'tools/RecompOne/RecompOne.Runtime/Media/DreamcastMovies.cs').read_text();hook=(ROOT/'spiderman/patches/DreamcastMoviePatches.cs').read_text();audio=(ROOT/'tools/RecompOne/RecompOne.Runtime/Host/Audio.Movies.cs').read_text()
  for text in ['DllNotFoundException','MovieOverrideResult.Failed','ClearMovieFrame','FrameClock.Resync','Runtime.PresentFrame','HoldForHostMovie']:self.assertIn(text,player)
  self.assertIn('func_8002B1FC',hook);self.assertIn('_sfd.ReadAudio',audio);self.assertNotIn('MovieCache',audio)
if __name__=='__main__':unittest.main(verbosity=2)
