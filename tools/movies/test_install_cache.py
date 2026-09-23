#!/usr/bin/env python3
"""Regression: retired cache tools refuse work; no new disc installer is added."""
import subprocess,sys,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
class RetirementTests(unittest.TestCase):
 def test_retired_tools_refuse_without_writing(self):
  with tempfile.TemporaryDirectory() as t:
   for name in ['movie_cache.py','install_cache.py']:
    p=subprocess.run([sys.executable,str(HERE/name),'--pack','old.zip','--game-dir',t],capture_output=True,text=True,cwd=t)
    self.assertEqual(p.returncode,2);self.assertIn('original unchanged .SFD',p.stderr);self.assertEqual(list(Path(t).iterdir()),[])
 def test_no_cache_reader_implementation(self):
  p=HERE.parents[1]/'tools/RecompOne/RecompOne.Runtime/Media/MovieCache.cs'
  self.assertNotIn('class MovieCache',p.read_text())
if __name__=='__main__':unittest.main(verbosity=2)
