// Original Dreamcast SFD -> MPEG-1 / ADX -> YUV420 / PCM16, in-process.
// MPEG-1 VLC tables and reconstruction adapted from PL_MPEG (MIT),
// Dominic Szablewski. See LICENSE-PL_MPEG.txt and SOURCES.md.
// OpenSpidey additions: bounded streaming PS reader, ADX, explicit errors,
// caller-owned allocation, full-picture validation, EOF reference flush,
// 64-bit IDCT intermediates, edge-safe prediction and a small stable C ABI.
#include "sfd_decoder.h"
#include "mpeg1_vlc.h"
using B=SfdU8; using U=SfdU32; using Q=SfdU64; using I64=long long;
#if defined(_WIN32)
extern "C" { int _fltused = 0; }
#endif
static_assert(sizeof(U)==4 && sizeof(Q)==8 && sizeof(short)==2, "unsupported ABI");
static constexpr U MaxWidth=720, MaxHeight=576, MaxPixels=MaxWidth*MaxHeight;
static constexpr U MaxFrames=108000, CacheBytes=32768;
static constexpr U Magic=0x31444653;
static void copy(B*d,const B*s,U n){for(U i=0;i<n;i++)d[i]=s[i];}
static void clear(void*p,U n){B*d=(B*)p;for(U i=0;i<n;i++)d[i]=0;}
static U be16(const B*p){return U(p[0])*256+p[1];}
static U be32(const B*p){return U(p[0])*16777216+U(p[1])*65536+U(p[2])*256+p[3];}
static int clip(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static B pixel(I64 x){return B(x<0?0:x>255?255:x);}
struct Input {
 SfdReadAt read; void* user; Q size; int error;
 int fail(int e){if(!error)error=e;return 0;}
};
struct Reader {
 Input* input; Q base; U length; B data[CacheBytes];
 bool ensure(Q pos) {
  if(input->error)return false;
  if(pos>=input->size){input->fail(-5);return false;}
  if(length && pos>=base && pos-base<length)return true;
  base=pos-(pos%CacheBytes);length=0;
  U need=U(input->size-base<CacheBytes?input->size-base:CacheBytes);
  while(length<need){
   int n=input->read(input->user,base+length,data+length,need-length);
   if(n<=0 || U(n)>need-length){input->fail(n<0?-2:-5);return false;}
   length+=U(n);
  }
  return true;
 }
 B at(Q pos){return ensure(pos)?data[U(pos-base)]:0;}
 U read(Q pos,B*dst,U n){
  U done=0;
  if(pos>input->size || n>input->size-pos){input->fail(-5);return 0;}
  while(done<n && ensure(pos+done)){
   U off=U(pos+done-base),part=length-off;
   if(part>n-done)part=n-done;
   copy(dst+done,data+off,part);done+=part;
  }
  return done;
 }
};
struct Stream {
 Reader r; Q pos,payload,remaining; int wanted; bool ended,sofdec;
 bool next(){
  Input& in=*r.input;
  Q scanStart=pos;
  while(!ended && !in.error){
   if(pos-scanStart>8*1024*1024){in.fail(-8);return false;}
   if(pos>in.size || in.size-pos<4){in.fail(-5);return false;}
   B h[16];if(r.read(pos,h,4)!=4)return false;
   if(h[0] || h[1] || h[2]!=1){in.fail(-3);return false;}
   int sid=h[3];Q start=pos;
   if(sid==0xb9){
    // End code may be followed by a sector's padding, never arbitrary data.
    Q tail=pos+4;
    if(in.size-tail>2048){in.fail(-3);return false;}
    for(;tail<in.size;tail++)if(r.at(tail)!=0xff){in.fail(-3);return false;}
    pos=in.size;ended=true;return false;
   }
   if(sid==0xba){
    if(in.size-pos<12 || r.read(pos,h,12)!=12){in.fail(-5);return false;}
    // This title uses MPEG-1 pack headers. Reject MPEG-2/Sofdec2, not guess.
    if((h[4]&0xf1)!=0x21 || !(h[6]&1) || !(h[8]&1) || !(h[9]&0x80) || !(h[11]&1)){
     in.fail(-4);return false;
    }
    pos+=12;continue;
   }
   if(in.size-pos<6 || r.read(pos,h,6)!=6){in.fail(-5);return false;}
   U count=be16(h+4);
   if(!count || count>in.size-pos-6){in.fail(-5);return false;}
   pos+=6+count;
   if(sid==0xbf){
    // Authoritative SofdecStream signature inside private_stream_2.
    static const B mark[]={'S','o','f','d','e','c','S','t','r','e','a','m'};
    for(U off=0;off+12<=count && off<64;off++){
     bool same=true;for(U j=0;j<12;j++)if(r.at(start+6+off+j)!=mark[j]){same=false;break;}
     if(same){sofdec=true;break;}
    }
    continue;
   }
   if(sid==0xbb || sid==0xbe)continue;
   // One unencrypted ADX stream and one MPEG-1 video stream are supported.
   if(sid!=0xc0 && sid!=0xe0){in.fail(-4);return false;}
   Q p=start+6,end=pos;
   U stuffing=0;
   while(p<end && r.at(p)==0xff){p++;if(++stuffing>32){in.fail(-3);return false;}}
   if(p>=end){in.fail(-3);return false;}
   if((r.at(p)&0xc0)==0x40){p+=2;if(p>=end){in.fail(-3);return false;}}
   int lead=r.at(p);U timeBytes=0;
   if((lead&0xf0)==0x20)timeBytes=5;
   else if((lead&0xf0)==0x30)timeBytes=10;
   else if(lead==0x0f)timeBytes=1;
   else{in.fail(-4);return false;}
   if(timeBytes>end-p){in.fail(-5);return false;}
   if(timeBytes>1){
    if(!(r.at(p)&1) || !(r.at(p+2)&1) || !(r.at(p+4)&1)) {in.fail(-3);return false;}
    if(timeBytes==10 && ((r.at(p+5)&0xf1)!=0x11 || !(r.at(p+7)&1) || !(r.at(p+9)&1))) {in.fail(-3);return false;}
   }
   p+=timeBytes;
   if(sid!=wanted)continue;
   if(!sofdec){in.fail(-4);return false;}
   payload=p;remaining=end-p;
   if(remaining)return true;
  }
  return false;
 }
 U read(B*d,U n){
  U done=0;
  while(done<n && !r.input->error){
   if(!remaining && !next())break;
   U take=U(remaining<n-done?remaining:n-done);
   if(r.read(payload,d+done,take)!=take)break;
   payload+=take;remaining-=take;done+=take;
  }
  return done;
 }
};
struct Bits {
 Stream* stream; Q shift,consumed; U count,index,length; B data[CacheBytes]; bool eof;
 bool ensure(U n){
  if(n>32){stream->r.input->fail(-8);return false;}
  while(count<n && !stream->r.input->error){
   if(index==length){index=0;length=stream->read(data,CacheBytes);if(!length){eof=true;return false;}}
   shift=(shift<<8)|data[index++];count+=8;
  }
  return !stream->r.input->error;
 }
 U peek(U n){if(!ensure(n)){stream->r.input->fail(-5);return 0;}return U((shift>>(count-n))&((Q(1)<<n)-1));}
 U get(U n){U v=peek(n);if(count>=n){count-=n;consumed+=n;}return v;}
 void skip(U n){while(n>32){get(32);n-=32;}if(n)get(n);}
 int code(){
  if(count%8)get(count%8);
  U state=0xffffff;
  for(U scanned=0;scanned<2*1024*1024;scanned++){
   if(!ensure(8))return -1;
   U c=get(8);if(state==1)return int(c);
   state=((state<<8)|c)&0xffffff;
  }
  stream->r.input->fail(-8);return -1;
 }
 template<U N>int vlc(const Vlc(&table)[N]){
  int node=0;
  for(U depth=0;depth<32;depth++){
   int at=node+int(get(1));if(stream->r.input->error || at<0 || at>=int(N)){stream->r.input->fail(-6);return 0;}
   const Vlc& v=table[at];if(v.next==0)return v.value;
   if(v.next<0){stream->r.input->fail(-6);return 0;}node=v.next;
  }
  stream->r.input->fail(-6);return 0;
 }
};
static const B Zig[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
static const B IntraQuant[64]={8,16,19,22,26,27,29,34,16,16,22,24,27,29,34,37,19,22,26,27,29,34,34,38,22,22,26,27,29,34,37,40,22,26,27,29,32,35,40,48,26,27,29,32,35,40,48,58,26,27,29,34,38,46,56,69,27,29,35,38,46,56,69,83};
struct MotionState {int full,set,r,h,v;};
struct Video {
 Bits bits; SfdInfo* info; B storage[MaxPixels*9/2]; B *current,*forward,*backward;
 U width,height,mw,mh,lw,lh,frames,refs,covered; int pending,type,quant,address,row,col; bool initialized,done,pendingRef,intra;
 B iq[64],nq[64],coverage[MaxPixels/256];int dc[3];I64 block[64];MotionState fw,bw;
 Input& input(){return *bits.stream->r.input;}
 bool sequence(){
  U w=bits.get(12),h=bits.get(12),aspect=bits.get(4),rate=bits.get(4);
  static const U fpsN[9]={0,24000,24,25,30000,30,50,60000,60};
  static const U fpsD[9]={1,1001,1,1,1001,1,1,1001,1};
  // MPEG-1 table is pel height/width; expose width/height SAR, not its inverse.
  static const U pel[15]={0,10000,6735,7031,7615,8055,8437,8935,9157,9815,10255,10695,10950,11575,12015};
  if(!w || !h || (w&1) || (h&1) || w>MaxWidth || h>MaxHeight || !aspect || aspect>14 || !rate || rate>8){input().fail(-4);return false;}
  if(initialized && (w!=width || h!=height || fpsN[rate]!=info->fps_num || fpsD[rate]!=info->fps_den || pel[aspect]!=info->sar_den)){input().fail(-4);return false;}
  bits.skip(18);if(bits.get(1)!=1){input().fail(-6);return false;}bits.skip(11);
  bool custom=bits.get(1)!=0;
  for(U i=0;i<64;i++){U j=Zig[i];iq[j]=custom?B(bits.get(8)):IntraQuant[j];if(!iq[j])input().fail(-6);}
  custom=bits.get(1)!=0;
  for(U i=0;i<64;i++){U j=Zig[i];nq[j]=custom?B(bits.get(8)):16;if(!nq[j])input().fail(-6);}
  if(input().error)return false;
  width=w;height=h;mw=(w+15)/16;mh=(h+15)/16;lw=mw*16;lh=mh*16;
  info->width=w;info->height=h;info->fps_num=fpsN[rate];info->fps_den=fpsD[rate];info->sar_num=10000;info->sar_den=pel[aspect];
  if(!initialized){U bytes=lw*lh*3/2;current=storage;forward=storage+bytes;backward=storage+2*bytes;initialized=true;}
  return true;
 }
 bool open(){
  for(U n=0;n<64;n++){
   int c=bits.code();if(c==0xb3){pending=-1;return sequence();}
   if(c<0)break;
  }
  input().fail(-6);return false;
 }
 int vector(int r,int old){
  int code=bits.vlc(Motion),delta=code,f=1<<r;
  if(code && r){int magnitude=code<0?-code:code;delta=(magnitude-1)*f+int(bits.get(U(r)))+1;if(code<0)delta=-delta;}
  int now=old+delta;if(now>16*f-1)now-=32*f;else if(now< -16*f)now+=32*f;return now;
 }
 bool mark(){
  if(address<0 || address>=int(mw*mh) || coverage[address]){input().fail(-6);return false;}
  coverage[address]=1;covered++;row=address/int(mw);col=address%int(mw);return true;
 }
 void predictionPlane(B*src,B*dst,int stride,int rows,int bs,int mx,int my,bool blend){
  // Edge emulation is explicit in X and Y; no linear-index wrap into another row.
  int x0=col*bs+(mx>>1),y0=row*bs+(my>>1),ox=mx&1,oy=my&1;
  int target=(row*stride+col)*bs;
  for(int y=0;y<bs;y++){
   int y1=clip(y0+y,0,rows-1),y2=clip(y0+y+oy,0,rows-1);
   for(int x=0;x<bs;x++){
    int x1=clip(x0+x,0,stride-1),x2=clip(x0+x+ox,0,stride-1);
    int value;
    if(ox && oy)value=(src[y1*stride+x1]+src[y1*stride+x2]+src[y2*stride+x1]+src[y2*stride+x2]+2)>>2;
    else if(ox)value=(src[y1*stride+x1]+src[y1*stride+x2]+1)>>1;
    else if(oy)value=(src[y1*stride+x1]+src[y2*stride+x1]+1)>>1;
    else value=src[y1*stride+x1];
    int at=target+y*stride+x;dst[at]=B(blend?(dst[at]+value+1)>>1:value);
   }
  }
 }
 void prediction(B*src,int mx,int my,bool blend){
  U y=lw*lh,c=y/4;
  predictionPlane(src,current,int(lw),int(lh),16,mx,my,blend);
  predictionPlane(src+y,current+y,int(lw/2),int(lh/2),8,mx/2,my/2,blend);
  predictionPlane(src+y+c,current+y+c,int(lw/2),int(lh/2),8,mx/2,my/2,blend);
 }
 void predict(){
  int x=fw.h*(fw.full?2:1),y=fw.v*(fw.full?2:1);
  if(type==3){
   int bx=bw.h*(bw.full?2:1),by=bw.v*(bw.full?2:1);
   if(fw.set){prediction(forward,x,y,false);if(bw.set)prediction(backward,bx,by,true);}
   else prediction(backward,bx,by,false);
  }else prediction(forward,x,y,false);
 }
 void idct(){
  // Separable MPEG inverse DCT with Q14 cosine basis. Coefficients remain in
  // their specified dequantized domain: no lossy 8-bit premultiplication table.
  // The two passes use 64-bit accumulation and one final round, /4 * 2^-28.
  static const int basis[8][8]={
   {11585,16069,15137,13623,11585,9102,6270,3196},
   {11585,13623,6270,-3196,-11585,-16069,-15137,-9102},
   {11585,9102,-6270,-16069,-11585,3196,15137,13623},
   {11585,3196,-15137,-9102,11585,13623,-6270,-16069},
   {11585,-3196,-15137,9102,11585,-13623,-6270,16069},
   {11585,-9102,-6270,16069,-11585,-3196,15137,-13623},
   {11585,-13623,6270,3196,-11585,16069,-15137,9102},
   {11585,-16069,15137,-13623,11585,-9102,6270,-3196}};
  I64 tmp[64];
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){
   I64 sum=0;for(int k=0;k<8;k++)sum+=block[y*8+k]*basis[x][k];tmp[y*8+x]=sum;
  }
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){
   I64 sum=0;for(int k=0;k<8;k++)sum+=tmp[k*8+x]*basis[y][k];block[y*8+x]=(sum+(I64(1)<<29))>>30;
  }
 }
 bool decodeBlock(int which){
  for(int i=0;i<64;i++)block[i]=0;
  int n=0;B*matrix=nq;
  if(intra){
   int plane=which>3?which-3:0;
   int size=plane?bits.vlc(DcC):bits.vlc(DcY);
   if(input().error)return false;
   int diff=0;if(size){int v=int(bits.get(U(size)));diff=(v&(1<<(size-1)))?v:v+1-(1<<size);}
   dc[plane]+=diff;
   if(dc[plane]< -2048 || dc[plane]>2047){input().fail(-6);return false;}
   block[0]=I64(dc[plane])*8;matrix=iq;n=1;
  }
  bool end=false;
  for(int steps=0;steps<65 && !input().error;steps++){
   int coeff=bits.vlc(Coeff),run=0,level=0;
   if(coeff==1 && n>0 && !bits.get(1)){end=true;break;}
   if(coeff==0xffff){
    run=int(bits.get(6));level=int(bits.get(8));
    if(!level)level=int(bits.get(8));else if(level==128)level=int(bits.get(8))-256;else if(level>128)level-=256;
    if(!level){input().fail(-6);return false;}
   }else{run=coeff>>8;level=coeff&255;if(bits.get(1))level=-level;}
   n+=run;if(n>=64 || n<0){input().fail(-6);return false;}
   int index=Zig[n++];level*=2;if(!intra)level+=level<0?-1:1;
   // Dequantize the magnitude, then restore sign. Arithmetic shift of a
   // negative product produces a systematic bias in long prediction chains.
   int sign=level<0?-1:1;
   level=((level<0?-level:level)*quant*matrix[index])>>4;
   if(level && !(level&1))level--;
   level*=sign;
   level=clip(level,-2048,2047);block[index]=level;
  }
  if(!end || input().error){input().fail(-6);return false;}
  int stride=int(lw),offset=(row*int(lw)+col)*16;B*dest=current;
  if(which<4){if(which&1)offset+=8;if(which&2)offset+=int(lw)*8;}
  else{stride=int(lw/2);offset=(row*stride+col)*8;dest+=lw*lh;if(which==5)dest+=lw*lh/4;}
  // storage order is Y, U, V, matching native output, not PL_MPEG's Y, V, U.
  if(n==1){I64 v=(block[0]+4)>>3;for(int y=0;y<8;y++)for(int x=0;x<8;x++){int at=offset+y*stride+x;dest[at]=pixel((intra?0:dest[at])+v);}}
  else{idct();for(int y=0;y<8;y++)for(int x=0;x<8;x++){int at=offset+y*stride+x;dest[at]=pixel((intra?0:dest[at])+block[y*8+x]);}}
  return true;
 }
 bool macroblock(bool first){
  int increment=0,t=0;
  for(U tries=0;tries<1024;tries++){
   t=bits.vlc(Address);if(input().error)return false;
   if(t==34)continue;
   if(t==35){increment+=33;if(increment>int(mw*mh)){input().fail(-6);return false;}continue;}
   increment+=t;break;
  }
  if(t<=0 || t>=34 || increment<=0 || address+increment>=int(mw*mh)){input().fail(-6);return false;}
  if(first){address+=increment;}
  else{
   if(increment>1){dc[0]=dc[1]=dc[2]=128;if(type==2)fw.h=fw.v=0;if(type==1){input().fail(-6);return false;}}
   while(increment>1){address++;if(!mark())return false;predict();increment--;}
   address++;
  }
  if(!mark())return false;
  int m=type==1?bits.vlc(TypeI):type==2?bits.vlc(TypeP):bits.vlc(TypeB);
  intra=(m&1)!=0;fw.set=m&8;bw.set=m&4;
  if(m&16)quant=int(bits.get(5));if(!quant){input().fail(-6);return false;}
  if(intra){fw.h=fw.v=bw.h=bw.v=0;}
  else{
   dc[0]=dc[1]=dc[2]=128;
   if(fw.set){fw.h=vector(fw.r,fw.h);fw.v=vector(fw.r,fw.v);}else if(type==2)fw.h=fw.v=0;
   if(bw.set){bw.h=vector(bw.r,bw.h);bw.v=vector(bw.r,bw.v);}
   if(input().error)return false;predict();
  }
  int pattern=(m&2)?bits.vlc(Pattern):intra?63:0;
  for(int block=0;block<6;block++)if(pattern&(32>>block))if(!decodeBlock(block))return false;
  return !input().error;
 }
 bool slice(int code){
  if(code<1 || code>int(mh)){input().fail(-6);return false;}
  address=(code-1)*int(mw)-1;fw.h=fw.v=bw.h=bw.v=0;fw.set=bw.set=0;dc[0]=dc[1]=dc[2]=128;
  quant=int(bits.get(5));if(!quant){input().fail(-6);return false;}
  for(U i=0;bits.get(1);i++){bits.skip(8);if(i>1024 || input().error){input().fail(-6);return false;}}
  bool first=true;
  do{
   if(!macroblock(first))return false;first=false;
   if(address==int(mw*mh)-1)break;
  }while(bits.peek(23)!=0 && !input().error);
  return !input().error;
 }
 B* next(){
  if(done || input().error)return nullptr;
  for(U attempts=0;attempts<4;attempts++){
   int code=pending;pending=-1;
   U headers=0;
   while(code!=0 && !input().error){
    if(++headers>1024){input().fail(-8);return nullptr;}
    if(code==0xb3){if(!sequence())return nullptr;}
    else if(code==0xb5){input().fail(-4);return nullptr;}
    else if(code==0xb7 || (code<0 && bits.eof)){
     done=true;
     if(!input().error && pendingRef){pendingRef=false;return backward;}
     return nullptr;
    }
    code=bits.code();
   }
   if(input().error)return nullptr;
   bits.skip(10);type=int(bits.get(3));bits.skip(16);
   if(type<1 || type>3 || (type==2 && refs<1) || (type==3 && refs<2)){input().fail(-6);return nullptr;}
   if(type==2 || type==3){fw.full=int(bits.get(1));int f=int(bits.get(3));if(!f){input().fail(-6);return nullptr;}fw.r=f-1;}
   if(type==3){bw.full=int(bits.get(1));int f=int(bits.get(3));if(!f){input().fail(-6);return nullptr;}bw.r=f-1;}
   for(U i=0;bits.get(1);i++){bits.skip(8);if(i>1024 || input().error){input().fail(-6);return nullptr;}}
   B* temp=forward;if(type!=3)forward=backward;
   clear(coverage,mw*mh);covered=0;
   code=bits.code();while(code==0xb2)code=bits.code();
   if(code==0xb5){input().fail(-4);return nullptr;}
   while(code>=1 && code<=0xaf){
    if(!slice(code))return nullptr;
    if(covered==mw*mh){code=-1;break;}
    code=bits.code();
   }
   pending=code;
   if(covered!=mw*mh || input().error){input().fail(-6);return nullptr;}
   if(type==3)return current;
   backward=current;current=temp;if(refs<2)refs++;
   bool had=pendingRef;pendingRef=true;if(had)return forward;
  }
  input().fail(-6);return nullptr;
 }
};
static double cosine(double x){double sum=1,term=1;for(int i=1;i<=18;i++){term*=-(x*x)/double((2*i-1)*(2*i));sum+=term;}return sum;}
static int roundEven(float f){int i=int(f);float r=f-float(i);if(r>.5f || (r==.5f && (i&1)))i++;if(r<-.5f || (r==-.5f && (i&1)))i--;return i;}
struct Adx {
 Stream stream;SfdInfo*info;U produced,used,ready;int coeff[2],history[2][2];short pcm[64];bool finished;
 Input& input(){return *stream.r.input;}
 bool open(){
  B h[24];if(stream.read(h,24)!=24){input().fail(-7);return false;}
  U offset=be16(h+2)+4,sr=be32(h+8),samples=be32(h+12),cutoff=be16(h+16),channels=h[7];
  if(be16(h)!=0x8000 || offset<24 || offset>4096 || h[4]!=3 || h[5]!=18 || h[6]!=4 ||
     (channels!=1 && channels!=2) || sr<8000 || sr>48000 || !samples || samples>sr*1800 ||
     !cutoff || cutoff>=sr/2 || h[18]!=3 || h[19]!=0){input().fail(-4);return false;}
  B tail[6]={};
  for(U i=24;i<offset;i++){
   B v;if(stream.read(&v,1)!=1){input().fail(-7);return false;}
   if(i>=offset-6)tail[i-(offset-6)]=v;
  }
  static const B marker[]={'(','c',')','C','R','I'};
  // Very short headers place the marker partly in the first 24 bytes.
  if(offset<30)for(U i=0;i<6 && offset-6+i<24;i++)tail[i]=h[offset-6+i];
  for(U i=0;i<6;i++)if(tail[i]!=marker[i]){input().fail(-7);return false;}
  const double root2=1.4142135623730950488;
  double a=root2-cosine(6.2831853071795864769*double(cutoff)/double(sr)),b=root2-1;
  double c=(a-__builtin_sqrt((a+b)*(a-b)))/b;
  coeff[0]=roundEven(float(c*8192.0));coeff[1]=roundEven(float(-c*c*4096.0));
  info->sample_rate=sr;info->channels=channels;info->audio_frames=samples;return !input().error;
 }
 bool block(){
  if(produced>=info->audio_frames)return false;
  B encoded[36];U bytes=18*info->channels;
  if(stream.read(encoded,bytes)!=bytes){input().fail(-7);return false;}
  for(U channel=0;channel<info->channels;channel++){
   const B*p=encoded+18*channel;int scale=int(be16(p));
   if(scale&0x8000){input().fail(-7);return false;}
   for(U s=0;s<32;s++){
    int residual=(p[2+s/2]>>((s&1)?0:4))&15;if(residual>=8)residual-=16;
    I64 prediction=(I64(coeff[0])*history[channel][0]+I64(coeff[1])*history[channel][1])>>12;
    int value=clip(int(prediction)+residual*scale,-32768,32767);
    history[channel][1]=history[channel][0];history[channel][0]=value;pcm[s*2+channel]=short(value);
   }
  }
  if(info->channels==1)for(U s=0;s<32;s++)pcm[s*2+1]=pcm[s*2];
  used=0;ready=info->audio_frames-produced<32?info->audio_frames-produced:32;return true;
 }
 bool finish(){
  if(finished)return !input().error;
  // Validate that the declared sample count ends at the ADX terminator rather
  // than treating a truncated stream or an arbitrary early count as success.
  B tail[4];if(stream.read(tail,4)!=4 || be16(tail)!=0x8001){input().fail(-7);return false;}
  U padding=be16(tail+2);if(padding>65532){input().fail(-7);return false;}
  B scratch[64];while(padding){U n=padding<64?padding:64;if(stream.read(scratch,n)!=n){input().fail(-7);return false;}padding-=n;}
  // Remaining stream bytes are sector padding. SFD audio PES can end on zeros.
  U extra=0,n;
  while((n=stream.read(scratch,64))!=0){
   extra+=n;if(extra>65536){input().fail(-7);return false;}
   for(U i=0;i<n;i++)if(scratch[i]!=0 && scratch[i]!=0xff){input().fail(-7);return false;}
  }
  finished=true;return !input().error;
 }
 int read(short*out,U frames){
  U n=0;
  while(n<frames && produced<info->audio_frames && !input().error){
   if(used==ready && !block())break;
   U take=ready-used;if(take>frames-n)take=frames-n;
   for(U i=0;i<take*2;i++)out[n*2+i]=pcm[used*2+i];
   used+=take;produced+=take;n+=take;
  }
  if(produced==info->audio_frames && !finish())return input().error;
  return input().error?input().error:int(n);
 }
};
struct Workspace { U magic;Input input;SfdInfo info;Stream videoStream;Video video;Adx audio; };
static Workspace* workspace(void*p){if(!p || (Q(p)&15))return nullptr;Workspace*w=(Workspace*)p;return w->magic==Magic?w:nullptr;}
SFD_API U sfd_abi(){return 0x00010000;}
SFD_API U sfd_workspace_size(){return U((sizeof(Workspace)+15)&~Q(15));}
SFD_API int sfd_open(void*p,U bytes,SfdReadAt read,void*user,Q size,SfdInfo*info){
 if(!p || (Q(p)&15) || bytes<sfd_workspace_size() || !read || !info || size<2048 || size>512ULL*1024*1024)return -1;
 clear(p,sfd_workspace_size());Workspace&w=*(Workspace*)p;w.magic=Magic;w.input.read=read;w.input.user=user;w.input.size=size;
 w.videoStream.r.input=&w.input;w.videoStream.wanted=0xe0;w.video.bits.stream=&w.videoStream;w.video.info=&w.info;w.video.pending=-1;
 w.audio.stream.r.input=&w.input;w.audio.stream.wanted=0xc0;w.audio.info=&w.info;
 if(!w.audio.open() || !w.video.open())return w.input.error?w.input.error:-3;
 *info=w.info;return 0;
}
SFD_API int sfd_video(void*p,B*out,U capacity,U*index){
 Workspace*w=workspace(p);if(!w || !out || !index)return -1;if(w->input.error)return w->input.error;
 Video&v=w->video;U bytes=v.width*v.height*3/2;
 if(!v.initialized || capacity<bytes)return -1;
 B*frame=v.next();if(w->input.error)return w->input.error;if(!frame)return 0;
 if(v.frames>=MaxFrames){w->input.fail(-8);return w->input.error;}
 U plane=v.lw*v.lh,pos=0;
 for(U y=0;y<v.height;y++){copy(out+pos,frame+y*v.lw,v.width);pos+=v.width;}
 for(U c=0;c<2;c++)for(U y=0;y<v.height/2;y++){copy(out+pos,frame+plane+c*plane/4+y*v.lw/2,v.width/2);pos+=v.width/2;}
 *index=v.frames++;return 1;
}
SFD_API int sfd_audio(void*p,short*out,U frames){Workspace*w=workspace(p);if(!w || !out || !frames || frames>65536)return -1;if(w->input.error)return w->input.error;return w->audio.read(out,frames);}
SFD_API int sfd_rgba(const B*yuv,U bytes,U width,U height,B*rgba,U capacity){
 if(!yuv || !rgba || !width || !height || width>MaxWidth || height>MaxHeight || (width&1) || (height&1) || bytes!=width*height*3/2 || capacity<width*height*4)return -1;
 U ySize=width*height;const B*u=yuv+ySize;const B*v=u+ySize/4;
 for(U row=0;row<height;row++)for(U col=0;col<width;col++){
  U i=row*width+col,c=(row/2)*(width/2)+col/2;int yy=int(yuv[i])-16,uu=int(u[c])-128,vv=int(v[c])-128;
  rgba[i*4]=pixel((298*yy+409*vv+128)>>8);rgba[i*4+1]=pixel((298*yy-100*uu-208*vv+128)>>8);rgba[i*4+2]=pixel((298*yy+516*uu+128)>>8);rgba[i*4+3]=255;
 }
 return 0;
}
SFD_API int sfd_error(void*p){Workspace*w=workspace(p);return w?w->input.error:-1;}
SFD_API void sfd_close(void*p){Workspace*w=workspace(p);if(w)w->magic=0;}
