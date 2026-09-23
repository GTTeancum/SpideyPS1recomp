#!/usr/bin/env python3
"""Decode original SFDs with the shipping native component and compare FFmpeg.
FFmpeg/NumPy are independent development references only, never runtime deps.
No media conversion is installed or written. Original SFD input is read-only.
"""
from __future__ import annotations
from pathlib import Path
import argparse,hashlib,json,subprocess,time
import numpy as np
from sfd_native import Decoder,HERE

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--movies',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--movie',action='append');ap.add_argument('--library',type=Path);a=ap.parse_args()
 a.out.mkdir(parents=True,exist_ok=True);rows=[]
 files=[a.movies/n for n in a.movie] if a.movie else sorted(p for p in a.movies.iterdir() if p.suffix.lower()=='.sfd')
 if not files:raise ValueError('no original SFD files')
 with (a.out/'ffmpeg.log').open('wb') as errors:
  for path in files:
   start=time.monotonic();source_hash=hashlib.sha256(path.read_bytes()).hexdigest()
   with Decoder(path,library=a.library) as d:
    info=d.info;nbytes=info['width']*info['height']*3//2;hs=[hashlib.sha256(),hashlib.sha256(),hashlib.sha256()];frames=0;total_abs=total_sq=pixels=maximum=0
    cmd=['ffmpeg','-v','error','-threads','1','-i',str(path),'-map','0:v:0','-vf','setpts=N/(FRAME_RATE*TB)','-fps_mode','passthrough','-pix_fmt','yuv420p','-f','rawvideo','pipe:1']
    with subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=errors) as proc:
     while True:
      native=d.video();reference=proc.stdout.read(nbytes)
      if native is None:assert not reference,(path,'missing native final frame');break
      assert len(reference)==nbytes,(path,'extra native frame or truncated reference')
      delta=np.frombuffer(native,np.uint8).astype(np.int16)-np.frombuffer(reference,np.uint8).astype(np.int16)
      total_abs+=int(np.abs(delta).sum());total_sq+=int((delta.astype(np.int32)**2).sum());pixels+=nbytes;maximum=max(maximum,int(np.abs(delta).max()));frames+=1
      hs[0].update(native);hs[1].update(reference)
     assert proc.wait()==0
    with subprocess.Popen(['ffmpeg','-v','error','-threads','1','-i',str(path),'-map','0:a:0','-af','asetpts=N/SR/TB','-ac','2','-c:a','pcm_s16le','-f','s16le','pipe:1'],stdout=subprocess.PIPE,stderr=errors) as proc:
     while True:
      native=d.audio()
      if native is None:break
      reference=proc.stdout.read(len(native));assert native==reference,(path,'PCM differs');hs[2].update(native)
     padding=proc.stdout.read();assert len(padding)<128,(path,'extra/missing ADX blocks');assert proc.wait()==0
    psnr=float(10*np.log10(255**2/(total_sq/pixels))) if total_sq else None
    assert frames>0 and d.audio_count==info['audio_frames']
    # IDCTs need not be bit-identical. Record EVERY difference, including the
    # explained full-pel B-skip reference difference; never label it exact YUV.
    assert total_abs/pixels<.1 and maximum<=32 and (psnr is None or psnr>60),(path,'unexpected video divergence')
    assert hashlib.sha256(path.read_bytes()).hexdigest()==source_hash
    row={'file':path.name,'source_sha256':source_hash,'source_bytes':path.stat().st_size,'video_frames':frames,'audio_frames':d.audio_count,'sample_rate':info['sample_rate'],'width':info['width'],'height':info['height'],'fps':[info['fps_num'],info['fps_den']],'sar':[info['sar_num'],info['sar_den']],
      'pcm_exact':True,'ffmpeg_final_block_padding_frames_omitted':len(padding)//4,'yuv_max_error':maximum,'yuv_mae':total_abs/pixels,'yuv_psnr':psnr,'native_yuv_sha256':hs[0].hexdigest(),'reference_yuv_sha256':hs[1].hexdigest(),'pcm_sha256':hs[2].hexdigest(),'fixed_workspace_bytes':d.workspace_bytes,'file_bytes_read':d.bytes,'source_unchanged':True,'seconds':time.monotonic()-start}
    rows.append(row);print(json.dumps(row),flush=True)
    (a.out/'original-sfd-results.json').write_text(json.dumps({'scope':'native decoder vs independent reference; NOT C# game playback','ffmpeg':subprocess.check_output(['ffmpeg','-version'],text=True).splitlines()[0],'movies':rows},indent=2)+'\n')
 return 0
if __name__=='__main__':raise SystemExit(main())
