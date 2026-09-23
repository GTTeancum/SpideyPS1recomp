// Bounded structured mutations; synthetic SFD only. ASan/UBSan, not gameplay.
#include "sfd_decoder.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
struct Source{std::vector<unsigned char> bytes;int shortRead,fail;unsigned calls;};
static int read(void*u,SfdU64 at,SfdU8*d,SfdU32 n){
 auto&s=*(Source*)u;if(++s.calls>8192)return -1;if(s.fail>=0 && at>=unsigned(s.fail))return -1;
 if(at>=s.bytes.size())return 0;
 unsigned take=unsigned(std::min<SfdU64>(n,s.bytes.size()-at));if(s.shortRead)take=std::min(take,unsigned(s.shortRead));
 std::memcpy(d,s.bytes.data()+at,take);return int(take);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> original((std::istreambuf_iterator<char>(f)),{});if(original.size()<2048)return 2;
 std::vector<unsigned char> raw(sfd_workspace_size()+32,0xAC);void*w=(void*)((reinterpret_cast<uintptr_t>(raw.data())+15)&~uintptr_t(15));
 std::vector<unsigned char> video(720*576*3/2+32,0xED);std::vector<short> audio(4096+16,short(0x5A5A));
 unsigned rng=0x53464406,opened=0,errors=0;auto random=[&](){rng=rng*1664525u+1013904223u;return rng;};
 for(unsigned iteration=0;iteration<10000;iteration++){
  Source s{original,0,-1,0};
  if(iteration%5==0)s.bytes.resize(random()%s.bytes.size());
  else for(unsigned j=0,n=1+iteration%8;j<n;j++){unsigned at=random()%s.bytes.size();if(iteration%3==0)at%=256;s.bytes[at]^=1u<<(random()%8);}
  if(iteration%23==0)s.shortRead=17;
  if(iteration%29==0)s.fail=int(random()%original.size());
  SfdInfo info{};int rc=sfd_open(w,sfd_workspace_size(),read,&s,s.bytes.size(),&info);
  if(rc==0){opened++;for(unsigned step=0;step<16;step++){
    SfdU32 frame=0;rc=(step%2==0)?sfd_video(w,video.data(),unsigned(video.size()-32),&frame):sfd_audio(w,audio.data(),2048);
    if(rc<0){errors++;break;}
   }}else errors++;
  for(unsigned i=unsigned(video.size()-32);i<video.size();i++)if(video[i]!=0xED)std::abort();
  for(unsigned i=4096;i<audio.size();i++)if(audio[i]!=short(0x5A5A))std::abort();
  sfd_close(w);
 }
 std::printf("{\"cases\":10000,\"opened\":%u,\"decoder_errors\":%u,\"max_operations_per_case\":16,\"canaries_intact\":true}\n",opened,errors);return 0;
}
