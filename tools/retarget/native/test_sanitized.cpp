// Standalone ASan/UBSan harness. Input: RTG2 blob(s), not the full game.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
extern "C" int rtg_validate(const unsigned char*,unsigned);
extern "C" int rtg_evaluate(const unsigned char*,unsigned,const float*,unsigned,float*,unsigned,float*,unsigned);
static unsigned u(const unsigned char*p){return p[0]|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
int main(int argc,char**argv){
 unsigned cases=0,rejected=0,accepted=0;uint32_t random=0x14b0339;
 for(int ai=1;ai<argc;ai++){
  std::ifstream in(argv[ai],std::ios::binary);std::vector<unsigned char>d((std::istreambuf_iterator<char>(in)),{});
  if(d.empty()||rtg_validate(d.data(),unsigned(d.size())))return 2;
  float driver[216]={};for(int i=0;i<18;i++)for(int j=0;j<3;j++){driver[i*12+j*4+j]=1;std::memcpy(&driver[i*12+j*4+3],&d[64+(i*3+j)*4],4);}
  std::vector<float>b(u(d.data()+12)*12),v(u(d.data()+16)*6);
  if(rtg_evaluate(d.data(),unsigned(d.size()),driver,0,b.data(),unsigned(b.size()),v.data(),unsigned(v.size())))return 3;
  for(int c=0;c<5000;c++){
   auto z=d;random^=random<<13;random^=random>>17;random^=random<<5;
   unsigned pos=random%unsigned(z.size());z[pos]^=1u<<(random%8);
   if(c<300)z.resize(c);
   int err=rtg_validate(z.data(),unsigned(z.size()));cases++;
   if(err)rejected++;else{
    accepted++;
    // Successful validation may describe a different but still bounded layout.
    b.resize(u(z.data()+12)*12);v.resize(u(z.data()+16)*6);
    (void)rtg_evaluate(z.data(),unsigned(z.size()),driver,0,b.data(),unsigned(b.size()),v.data(),unsigned(v.size()));
   }
  }
 }
 std::printf("ASan/UBSan: %u mutated/truncated blobs, %u rejected, %u structurally valid; no sanitizer findings.\n",cases,rejected,accepted);
 return 0;
}
