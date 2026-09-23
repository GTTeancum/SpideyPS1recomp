#!/usr/bin/env python3
"""Generate an authored two-second SFD fixture. FFmpeg is development-only.
The fixture contains testsrc2, not any game movie. Audio is authored silent ADX.
"""
from pathlib import Path
import argparse,subprocess,tempfile,struct,hashlib,json
HERE=Path(__file__).resolve().parent

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,default=HERE/'test-fixtures/tiny.sfd');a=ap.parse_args()
 with tempfile.TemporaryDirectory() as t:
  video=Path(t)/'test.m1v'
  cmd=['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=32x32:rate=30:duration=2','-an','-c:v','mpeg1video','-g','12','-bf','2','-q:v','3','-f','mpeg1video',str(video)]
  subprocess.run(cmd,check=True);encoded=video.read_bytes()
 count=88200;header=bytearray(32);struct.pack_into('>HHBBBBIIHBB',header,0,0x8000,28,3,18,4,2,44100,count,500,3,0);header[-6:]=b'(c)CRI'
 adx=bytes(header)+(b'\0\1'+b'\0'*16)*2*((count+31)//32)+b'\x80\1\0\0'
 pack=bytes.fromhex('000001ba2100010001800001')
 def packet(sid,payload):return b'\0\0\1'+bytes([sid])+struct.pack('>H',len(payload))+payload
 out=bytearray(pack+packet(0xbf,b'SofdecStream'+bytes(20)))
 for i in range(0,max(len(encoded),len(adx)),2000):
  for sid,data in [(0xe0,encoded),(0xc0,adx)]:
   part=data[i:i+2000]
   if part:out+=pack+packet(sid,b'\x0f'+part)
 out+=b'\0\0\1\xb9';a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(out)
 manifest={'source':'authored FFmpeg testsrc2 + authored silent ADX v3; no game data','generator_command':cmd,'video_frames':60,'audio_frames':count,'file_sha256':hashlib.sha256(out).hexdigest(),'size':len(out)}
 a.out.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
