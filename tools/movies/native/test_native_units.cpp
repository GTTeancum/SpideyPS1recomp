// Tests the production core directly. Not a game or a replacement implementation.
#include "sfd_decoder.cpp"
#include <cstdio>
#include <cmath>
#include <memory>
#include <algorithm>
#include <cstdlib>
static unsigned checks=0;
static void check(bool ok,const char*name){if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}++checks;}
int main(){
 auto v=std::make_unique<Video>();
 for(int dc=-2048;dc<=2047;dc+=31){
  for(int i=0;i<64;i++)v->block[i]=0;v->block[0]=dc;v->idct();
  for(int i=0;i<64;i++)check(std::abs(double(v->block[i])-double(dc)/8)<=1,"DC transform");
 }
 unsigned rng=65537;
 for(int trial=0;trial<64;trial++){
  I64 original[64];for(int k=0;k<64;k++){rng=rng*1664525u+1013904223u;v->block[k]=original[k]=int(rng%4096)-2048;}v->idct();
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){
   double expected=0;
   for(int j=0;j<8;j++)for(int i=0;i<8;i++)expected+=original[j*8+i]*(i?1:std::sqrt(.5))*(j?1:std::sqrt(.5))*std::cos((2*x+1)*i*3.141592653589793/16)*std::cos((2*y+1)*j*3.141592653589793/16)/4;
   check(std::abs(v->block[y*8+x]-expected)<1.1,"bounded Q14 IDCT vs cosine definition");
  }
 }
 v->lw=v->lh=16;v->col=v->row=0;v->current=v->storage;v->backward=v->storage+384;v->forward=v->storage+768;
 for(int y=0;y<16;y++)for(int x=0;x<16;x++)v->backward[y*16+x]=B(x*10);
 v->type=3;v->fw.set=0;v->bw.set=4;v->bw.h=1;v->bw.v=0;v->bw.full=1;v->predict();
 check(v->current[0]==10,"full-pel B motion remains full-pel when reused by skipped block");
 v->bw.full=0;v->predict();check(v->current[0]==5,"half-pel interpolation remains distinct");
 v->bw.h=-63;v->bw.v=-63;v->predict();check(v->current[0]==0 && v->current[255]==0,"edge clamp never wraps into another row");
 B yuv[6]={16,235,16,235,128,128},rgba[16];
 check(sfd_rgba(yuv,6,2,2,rgba,16)==0 && rgba[0]==0 && rgba[4]==255 && rgba[3]==255,"limited BT601 black/white");
 check(sfd_rgba(yuv,6,3,2,rgba,16)<0,"odd dimensions rejected");
 check(sfd_rgba(yuv,6,2,2,rgba,15)<0,"output capacity enforced");
 check(sfd_abi()==0x10000 && sfd_workspace_size()<2*1024*1024,"ABI and fixed workspace");
 check(roundEven(2.5f)==2 && roundEven(3.5f)==4 && roundEven(-2.5f)==-2 && roundEven(-3.5f)==-4,"ADX coefficient rounding ties");
 std::printf("%u production-core assertions passed\n",checks);return 0;
}
