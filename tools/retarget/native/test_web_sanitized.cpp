// New render-only export stress. ASan/UBSan; no full game or managed-code claim.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <cstdio>
extern "C" int rtg_validate(const unsigned char*,unsigned);
extern "C" int rtg_pose(const unsigned char*,unsigned,const float*,unsigned,float*,unsigned);
extern "C" int rtg_web_target(const unsigned char*,unsigned,const float*,unsigned,unsigned,const short*,const int*,const int*,unsigned,int*);
extern "C" int rtg_web_segment(const int*,const int*,const int*,const int*,unsigned,unsigned,int*);
static unsigned u(const unsigned char*p){return unsigned(p[0])|unsigned(p[1])<<8|unsigned(p[2])<<16|unsigned(p[3])<<24;}
static unsigned state=0x3300275;static unsigned next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(int argc,char**argv){unsigned calls=0,failures=0;
 for(int ai=1;ai<argc;ai++){
  std::ifstream f(argv[ai],std::ios::binary);std::vector<unsigned char>d((std::istreambuf_iterator<char>(f)),{});if(rtg_validate(d.data(),d.size()))return 2;
  std::vector<float>b(u(d.data()+12)*12);float driver[216]={};for(int i=0;i<18;i++)for(int j=0;j<3;j++){driver[i*12+j*4+j]=1;std::memcpy(&driver[i*12+j*4+3],d.data()+64+(i*3+j)*4,4);}
  if(rtg_pose(d.data(),d.size(),driver,0,b.data(),b.size()))return 3;
  short rot[9]={4096,0,0,0,4096,0,0,0,4096};int tr[3]={},pos[3]={};
  for(unsigned c=0;c<10000;c++){
   int start[3],end[3],tip[3],target[3],out[6];for(int j=0;j<3;j++){start[j]=int(next());end[j]=int(next());tip[j]=int(next());target[j]=int(next());}for(auto &x:out)x=9876543;
   unsigned n=next()%4100,i=n?next()%(n+1):0;int rc=rtg_web_segment(start,end,tip,target,i,n,out);calls++;
   if(rc){for(auto x:out)if(x!=9876543)return 4;failures++;}
   auto mutated=d;unsigned size=unsigned(mutated.size());if(c%2==0){mutated[next()%size]^=1u<<(next()%8);}if(c<300)size=c;
   unsigned count=c%9==0?next()%(b.size()+1):b.size();for(auto &x:out)x=9876543;
   rc=rtg_web_target(mutated.data(),size,b.data(),count,next()%19,rot,tr,pos,next()%3,out);calls++;
   if(rc){for(auto x:out)if(x!=9876543)return 5;failures++;}
  }
 }
 int x[6]={};short r[9]={};float b[12]={};
 if(!rtg_web_segment(nullptr,x,x,x,0,1,x)||!rtg_web_target(nullptr,0,b,12,10,r,x,x,0,x))return 6;
 std::printf("ASan/UBSan: %u new-export stress calls; %u safe atomic rejections; no sanitizer findings.\n",calls,failures);
 return 0;
}
