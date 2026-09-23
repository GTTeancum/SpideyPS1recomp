// Offline test harness. The named native functions are mechanically translated
// from the user's generated main.cs; projection records coordinates, not pixels.
#include <cstdint>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <array>
using uint=uint32_t;using ushort=uint16_t;using byte=uint8_t;using ulong=uint64_t;
extern "C" int rtg_validate(const byte*,uint);
extern "C" int rtg_pose(const byte*,uint,const float*,uint,float*,uint);
extern "C" int rtg_web_target(const byte*,uint,const float*,uint,uint,const short*,const int*,const int*,uint,int*);
extern "C" int rtg_web_segment(const int*,const int*,const int*,const int*,uint,uint,int*);
struct Context {union{uint r[32];struct{uint Zero,At,V0,V1,A0,A1,A2,A3,T0,T1,T2,T3,T4,T5,T6,T7,S0,S1,S2,S3,S4,S5,S6,S7,T8,T9,K0,K1,GP,SP,FP,RA;};};uint LO=0,HI=0;Context(){for(auto&x:r)x=0;SP=0x807ff000;}template<class...T>void SetDerived(int i,uint v,T...){r[i]=v;}};
struct Mem {std::vector<byte>b=std::vector<byte>(0x800000+1024);uint offset(uint p,uint n){uint q;if(p>=0x1f800000&&p<0x1f800400)q=0x800000+p-0x1f800000;else if(p>=0x80000000&&p<0x80800000)q=p-0x80000000;else throw std::runtime_error("Invalid guest address");if(q+n>b.size())throw std::runtime_error("RAM bounds");return q;}uint ReadU8(uint p){return b[offset(p,1)];}uint ReadU16(uint p){uint q=offset(p,2);return b[q]|(b[q+1]<<8);}uint ReadU32(uint p){return ReadU16(p)|(ReadU16(p+2)<<16);}void WriteU8(uint p,byte v){b[offset(p,1)]=v;}void WriteU16(uint p,ushort v){WriteU8(p,v);WriteU8(p+1,v>>8);}void WriteU32(uint p,uint v){WriteU16(p,v);WriteU16(p+2,v>>16);}};
namespace Gte {
 uint cr[32]={},dr[32]={};uint ReadControl(int i){return cr[i];}void WriteControl(int i,uint v){cr[i]=v;}void Write(int i,uint v){dr[i]=v;}void ReadTo(Context&c,int i,int d){c.r[i]=dr[d];}void StoreWord(Mem&m,uint p,int i){m.WriteU32(p,dr[i]);}
 void LoadWord(Mem&m,uint p,int i){dr[i]=m.ReadU32(p);}
 void Execute(uint op){if(op!=0x4a480012&&op!=0x4a4ba012)throw std::runtime_error("Unexpected GTE operation");
  int16_t mat[9];uint mx=(op>>17)&3,vx=(op>>15)&3,cv=(op>>13)&3;
  uint mo=mx==0?0:8,to=cv==0?5:13;for(int j=0;j<9;j++)mat[j]=(int16_t)(cr[mo+j/2]>>(16*(j%2)));
  int v[3];if(vx==0){v[0]=(int16_t)dr[0];v[1]=(int16_t)(dr[0]>>16);v[2]=(int16_t)dr[1];}else for(int j=0;j<3;j++)v[j]=(int32_t)dr[9+j];
  for(int r=0;r<3;r++){int64_t x=int64_t((int32_t)cr[to+r])*4096;for(int j=0;j<3;j++)x+=int64_t(mat[r*3+j])*v[j];x>>=12;dr[25+r]=(uint)x;dr[9+r]=(uint)std::clamp<int64_t>(x,-32768,32767);}
 }
}
void func_80010614(Context&,Mem&){throw std::runtime_error("This socket fixture must use the full original cached pose");}
void func_80074310(Context&,Mem&){throw std::runtime_error("Partial 0x2A pose is not a valid oracle fixture");}
void func_80065D3C(Context&,Mem&);void func_80065E64(Context&,Mem&);void func_80065AC0(Context&,Mem&);void func_8007FA00(Context&,Mem&);void func_80073E18(Context&,Mem&);void func_8002CDFC(Context&,Mem&);void ptr_8002CEE4(Context&,Mem&);
static std::vector<std::array<int,6>> captured;
static bool corrected=false;static int target[3];static uint correctedCalls=0;
void func_80080988(Context&c,Mem&m){std::array<int,6>v;for(uint j=0;j<6;j++)v[j]=(int)m.ReadU32(0x1f800000+j*4);captured.push_back(v);c.V0=0xffffffff;}
void webhook(Context&c,Mem&m){if(!corrected)return;auto old=c;uint cr[32],dr[32];std::memcpy(cr,Gte::cr,sizeof cr);std::memcpy(dr,Gte::dr,sizeof dr);
 uint n=m.ReadU32(c.S0+0x3c),p=m.ReadU32(c.S0+0x40),i=c.S6;int a[3],b[3],tip[3],out[6];
 for(uint j=0;j<3;j++){a[j]=(int)m.ReadU32((i? p+(i-1)*16:c.S0+0x44)+j*4);b[j]=(int)m.ReadU32(p+i*16+j*4);tip[j]=(int)m.ReadU32(p+(n-1)*16+j*4);}
 if(rtg_web_segment(a,b,tip,target,i,n,out))throw std::runtime_error("Core segment rejected");for(uint j=0;j<6;j++)m.WriteU32(0x1f800000+j*4,out[j]);
 if(std::memcmp(&old,&c,sizeof c)||std::memcmp(cr,Gte::cr,sizeof cr)||std::memcmp(dr,Gte::dr,sizeof dr))throw std::runtime_error("Hook clobbered CPU/GTE");correctedCalls++;
}
std::vector<byte> read(const char*p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing input");return std::vector<byte>(std::istreambuf_iterator<char>(f),{});}
static uint u(const byte*p){return uint(p[0])|uint(p[1])<<8|uint(p[2])<<16|uint(p[3])<<24;}
// INSERT_ORIGINAL_ROUTINES_HERE
int main(int argc,char**argv){try{
 if(argc!=4)throw std::runtime_error("Usage: web-oracle rig.rtg native-animation.bin capture.bin");auto rig=read(argv[1]),bank=read(argv[2]);if(rtg_validate(rig.data(),rig.size()))throw std::runtime_error("Bad rig");
 Mem m;const uint actor=0x80300000,pose=0x80400000,line=0x80500000,points=0x80510000,sockets=0x80520000,list=0x80530000,begin=0x80530010,end=0x80530020,socketOut=0x80530030;
 m.WriteU16(actor,4);m.WriteU8(actor+0x1b,0);m.WriteU32(actor+0x180,pose);m.WriteU32(0x800a0924,sockets);m.WriteU16(sockets+6,10);m.WriteU16(sockets+14,5);
 short rot[9]={4096,0,0,0,4096,0,0,0,4096};int tr[3]={},pos[3]={};for(uint i=0;i<9;i++)m.WriteU16(actor+0x160+i*2,rot[i]);
 m.WriteU32(line+0x3c,16);m.WriteU32(line+0x40,points);m.WriteU32(list,line);const int anchor[3]={-600*256,-6500*256,-200*256};for(uint j=0;j<3;j++)m.WriteU32(begin+j*4,anchor[j]);
 std::ofstream out(argv[3],std::ios::binary);out.write("WCP3",4);uint records=115;out.write((char*)&records,4);uint count=16;out.write((char*)&count,4);
 size_t cursor=8;uint written=0;std::vector<float>bones(u(rig.data()+12)*12);float drivers[216];
 for(uint clip=0;clip<u(bank.data()+4);clip++){if(cursor+8>bank.size())throw std::runtime_error("Bank header bounds");uint ci=u(bank.data()+cursor),nf=u(bank.data()+cursor+4);cursor+=8;if(cursor+size_t(nf)*432>bank.size())throw std::runtime_error("Bank pose bounds");
  for(uint frame=0;frame<nf;frame++,cursor+=432){if(ci!=275&&ci!=280)continue;
   std::copy_n(bank.data()+cursor,432,m.b.data()+m.offset(pose,432));
   for(uint i=0;i<18;i++)for(uint r=0;r<3;r++){for(uint k=0;k<3;k++)drivers[i*12+r*4+k]=(int16_t)m.ReadU16(pose+i*24+(r*3+k)*2)/4096.f;drivers[i*12+r*4+3]=(int16_t)m.ReadU16(pose+i*24+18+r*2);}
   uint part=ci==280?5:10;if(rtg_pose(rig.data(),rig.size(),drivers,0,bones.data(),bones.size()))throw std::runtime_error("Pose core failed");if(rtg_web_target(rig.data(),rig.size(),bones.data(),bones.size(),part,rot,tr,pos,0,target))throw std::runtime_error("Socket core failed");
   Context c;c.A0=socketOut;c.A1=actor;c.A2=part==5?1:0;func_80073E18(c,m);
   // Reproduce native hand-8*forward at 80046444. The fixture's declared forward
   // is +Z Q12; this does not manufacture a vertical gap or alter the native Y.
   for(uint j=0;j<3;j++)m.WriteU32(end+j*4,m.ReadU32(socketOut+j*4)-(j==2?8*4096:0));
   c=Context();c.A0=line;c.A1=begin;c.A2=end;func_8002CDFC(c,m);
   // Native line input, not a made-up web socket. No additional flutter in this
   // controlled fixture; arbitrary flutter is covered separately in core tests.
   auto originalPoints=std::vector<byte>(m.b.begin()+m.offset(points,count*16),m.b.begin()+m.offset(points,count*16)+count*16);
   auto originalActor=std::vector<byte>(m.b.begin()+m.offset(actor,0x1000),m.b.begin()+m.offset(actor,0x1000)+0x1000);
   corrected=false;captured.clear();c=Context();c.A0=list;ptr_8002CEE4(c,m);auto before=captured;
   corrected=true;captured.clear();c=Context();c.A0=list;ptr_8002CEE4(c,m);auto after=captured;
   if(before.size()!=count||after.size()!=count)throw std::runtime_error("Native line loop lost a segment");
   if(!std::equal(originalPoints.begin(),originalPoints.end(),m.b.begin()+m.offset(points,count*16))||!std::equal(originalActor.begin(),originalActor.end(),m.b.begin()+m.offset(actor,0x1000)))throw std::runtime_error("Persistent points/actor changed");
   for(uint j=0;j<3;j++){if(after.front()[j]!=anchor[j]||after.back()[j+3]!=target[j])throw std::runtime_error("Anchor or final attachment mismatch");}
   for(uint i=1;i<count;i++)for(uint j=0;j<3;j++)if(after[i-1][j+3]!=after[i][j])throw std::runtime_error("Inter-segment seam");
   out.write((char*)&ci,4);out.write((char*)&frame,4);out.write((char*)&part,4);out.write((char*)target,12);out.write((char*)before.data(),before.size()*24);out.write((char*)after.data(),after.size()*24);written++;
  }
 }
 if(written!=records)throw std::runtime_error("Unexpected swing clip frame count");std::cout<<written<<" original swing poses; "<<correctedCalls<<" native line segments corrected; exact endpoint/anchor/continuity; original actor+line bytes and hook CPU/GTE unchanged.\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
