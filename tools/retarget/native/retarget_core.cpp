// OpenSpidey RTG2 weighted retargeter. No CRT, allocation or third-party runtime.
// Row-major affine 3x4 matrices, column vectors. C ABI for C# and proof harness.
#if defined(_WIN32)
#define API extern "C" __declspec(dllexport)
extern "C" { int _fltused = 0; }
#else
#define API extern "C" __attribute__((visibility("default")))
#endif
using U=unsigned int; using I=int; using B=unsigned char;
static U u(const B* p){return U(p[0])|(U(p[1])<<8)|(U(p[2])<<16)|(U(p[3])<<24);}
static I si(const B*p){return (I)u(p);}
static float f(const B*p){union {U u;float f;}v;v.u=u(p);return v.f;}
static bool finite(float x){union{float f;U u;}v;v.f=x;return (v.u&0x7f800000)!=0x7f800000;}
static bool range(U o,U n,U stride,U len){return o<=len && n<=(len-o)/stride;}
static void load(float* d,const B* s,U n){for(U i=0;i<n;i++)d[i]=f(s+4*i);}
static void mul(const float*a,const float*b,float*c){
 for(I r=0;r<3;r++)for(I k=0;k<4;k++){
  float t=k==3?a[r*4+3]:0;
  for(I j=0;j<3;j++)t+=a[r*4+j]*b[j*4+k];c[r*4+k]=t;
 }
}
static float det(const float*m){return m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);}
API U rtg_abi(){return 0x00020002;}
// 0 success; malformed files are rejected before any output/guest memory write.
API I rtg_validate(const B*d,U len){
 if(!d||len<280||u(d)!=0x32475452||(u(d+4)!=2&&u(d+4)!=3)||u(d+8)!=len)return 1;
 bool paged=u(d+4)==3;
 if(paged&&len<296)return 1;
 U packets=paged?u(d+280):18,mapping=paged?u(d+284):0;
 if(paged&&(packets<=18||packets>64||u(d+288)||u(d+292)||!range(mapping,packets,4,len)))return 19;
 if(paged)for(U i=0;i<packets;i++)if(u(d+mapping+i*4)!=(i<18?i:0))return 19;
 if(!finite(f(d+60))||f(d+60)<.05f||f(d+60)>8.f)return 18;
 U nb=u(d+12),nv=u(d+16),nw=u(d+20),anchor=u(d+24);
 U bo=u(d+28),vo=u(d+32),wo=u(d+36),po=u(d+40),jo=u(d+44),jl=u(d+48),to=u(d+52),nt=u(d+56);
 if(!nb||nb>256||!nv||nv>(paged?8192u:4096u)||nw>65536||anchor>=nb)return 2;
 if(!range(bo,nb,196,len)||!range(vo,nv,32,len)||!range(wo,nw,8,len)||!range(po,packets,8,len)||!range(jo,jl,1,len)||!range(to,nt,12,len))return 3;
 if(paged&&(bo<296||vo<bo+nb*196||wo<vo+nv*32||po<wo+nw*8||mapping<po+packets*8||to<mapping+packets*4||jo<to+nt*12))return 19;
 for(U i=0;i<54;i++)if(!finite(f(d+64+i*4)))return 4;
 for(U i=0;i<nb;i++){
  const B*b=d+bo+i*196;I p=si(b),driver=si(b+4);
  if(p< -1||p>=(I)i||driver< -1||driver>=18)return 5;
  for(U j=0;j<45;j++)if(!finite(f(b+16+j*4)))return 6;
  float m[12];load(m,b+16,12);float dt=det(m);if(dt<.9f||dt>1.1f)return 7;
 }
 for(U i=0;i<nv;i++){
  const B*v=d+vo+i*32;U first=u(v+24),count=u(v+28);float sum=0;
  if(!count||first>nw||count>nw-first)return 8;
  for(U j=0;j<6;j++)if(!finite(f(v+j*4)))return 9;
  for(U j=0;j<count;j++){
   const B*w=d+wo+(first+j)*8;float q=f(w+4);
   if(u(w)>=nb||!finite(q)||q<0||q>1.00001f)return 10;sum+=q;
  }
  if(sum<.9999f||sum>1.0001f)return 11;
 }
 for(U i=0;i<packets;i++){
  U count=u(d+po+i*8),off=u(d+po+i*8+4);if(count>256||!range(off,count,4,len))return 12;
  if(paged&&(off<mapping+packets*4||off>to||count>(to-off)/4))return 19;
  for(U j=0;j<count;j++)if(u(d+off+j*4)>=nv)return 13;
 }
 for(U i=0;i<nt*3;i++)if(u(d+to+i*4)>=nv)return 14;
 return 0;
}
// flags bit0: open fingers, bit1: source-bind reconstruction (validation only).
// scratch/output bones: nb*12 floats; output vertices: nv*6 floats (position,normal).
API I rtg_pose(const B*d,U len,const float*driver,U flags,float*bones,U boneFloats){
 I err=rtg_validate(d,len);if(err)return err;
 U nb=u(d+12),nv=u(d+16),anchor=u(d+24),bo=u(d+28),vo=u(d+32),wo=u(d+36);
 if(!driver||!bones||boneFloats<nb*12)return 15;
 for(U j=0;j<216;j++)if(!finite(driver[j]))return 16;
 float delta[3];for(U j=0;j<3;j++)delta[j]=(driver[j*4+3]-f(d+64+j*4))*f(d+60);
 for(U i=0;i<nb;i++){
  const B*b=d+bo+i*196;I parent=si(b),map=si(b+4);float*pose=bones+i*12;
  float rest[12],local[12];load(rest,b+64,12);load(local,b+112,12);
  if(parent>=0)mul(bones+parent*12,local,pose);
  else {for(U j=0;j<12;j++)pose[j]=rest[j];for(U j=0;j<3;j++)pose[j*4+3]+=delta[j];}
  if(i==anchor)for(U j=0;j<3;j++)pose[j*4+3]=rest[j*4+3]+delta[j];
  if(map>=0){
   const float*m=driver+map*12;
   for(I r=0;r<3;r++)for(I c=0;c<3;c++){
    float q=0;for(I k=0;k<3;k++)q+=m[r*4+k]*rest[k*4+c];pose[r*4+c]=q;
   }
  }
  if(!(flags&1)){
   float temp[9];for(U r=0;r<3;r++)for(U c=0;c<3;c++){
    float q=0;for(U k=0;k<3;k++)q+=pose[r*4+k]*f(b+160+4*(k*3+c));temp[r*3+c]=q;
   }
   for(U r=0;r<3;r++)for(U c=0;c<3;c++)pose[r*4+c]=temp[r*3+c];
  }
 }
 if(flags&2)for(U i=0;i<nb;i++){
  // Invert the stored rigid inverse bind. This still executes weighted skinning;
  // it is not a passthrough "identity test" that could conceal a bad bind matrix.
  float inv[12];load(inv,d+bo+i*196+16,12);float*pose=bones+i*12;
  for(U r=0;r<3;r++){
   for(U c=0;c<3;c++)pose[r*4+c]=inv[c*4+r];
   pose[r*4+3]=-(pose[r*4]*inv[3]+pose[r*4+1]*inv[7]+pose[r*4+2]*inv[11]);
  }
 }
 return 0;
}
API I rtg_evaluate(const B*d,U len,const float*driver,U flags,float*bones,U boneFloats,float*out,U outFloats){
 I err=rtg_validate(d,len);if(err)return err;
 U nv=u(d+16),bo=u(d+28),vo=u(d+32),wo=u(d+36);
 if(!out||outFloats<nv*6)return 15;
 err=rtg_pose(d,len,driver,flags,bones,boneFloats);if(err)return err;
 // Reuse per-vertex stack matrices instead of allocation; full variable influence list.
 for(U i=0;i<nv;i++){
  const B*v=d+vo+i*32;U first=u(v+24),count=u(v+28);float p[6];load(p,v,6);
  float result[6]={0,0,0,0,0,0};
  for(U j=0;j<count;j++){
   const B*w=d+wo+(first+j)*8;U bi=u(w);float weight=f(w+4),inv[12],skin[12];
   load(inv,d+bo+bi*196+16,12);mul(bones+bi*12,inv,skin);
   for(U r=0;r<3;r++){
    result[r]+=weight*(skin[r*4]*p[0]+skin[r*4+1]*p[1]+skin[r*4+2]*p[2]+skin[r*4+3]);
    result[r+3]+=weight*(skin[r*4]*p[3]+skin[r*4+1]*p[4]+skin[r*4+2]*p[5]);
   }
  }
  float n=result[3]*result[3]+result[4]*result[4]+result[5]*result[5];
  if(n>1e-12f){float a=1/__builtin_sqrtf(n);for(U j=3;j<6;j++)result[j]*=a;}
  for(U j=0;j<6;j++){if(!finite(result[j]))return 17;out[i*6+j]=result[j];}
 }
 return 0;
}
// Undo the exact quantized native part transform, not its approximate transpose.
// The ordinary PS1 path then reapplies that transform exactly once.
API I rtg_to_part(const float*m,const float*world,U count,float*local){
 if(!m||!world||!local||count>4096)return 1;
 float q=det(m);if(!finite(q)||q<.25f||q>4.f)return 2;
 float v[9]={m[5]*m[10]-m[6]*m[9],m[2]*m[9]-m[1]*m[10],m[1]*m[6]-m[2]*m[5],
 m[6]*m[8]-m[4]*m[10],m[0]*m[10]-m[2]*m[8],m[2]*m[4]-m[0]*m[6],
 m[4]*m[9]-m[5]*m[8],m[1]*m[8]-m[0]*m[9],m[0]*m[5]-m[1]*m[4]};
 for(U i=0;i<count;i++)for(U r=0;r<3;r++){
  float p=0,n=0;for(U k=0;k<3;k++){p+=v[r*3+k]*(world[i*6+k]-m[k*4+3]);n+=m[k*4+r]*world[i*6+3+k];}
  local[i*6+r]=p/q;local[i*6+3+r]=n;
 }
 return 0;
}

// Render-only swing attachment. Bone poses are the SAME preserved-rig poses used
// for skinning. Native driver aliases 6/11 refer to 5/10, not finger bones.
// actorR is the original 9 signed Q12 matrix elements, actorT its 3 int translation
// words, and actorPosition is the original actor+4 XYZ in world Q12. The 1/16
// conversion is exactly the final coordinate conversion in native func_80073E18.
// Double intermediates preserve precision at distant world coordinates.
API I rtg_web_target(const B*d,U len,const float*bones,U boneFloats,U part,
 const short*actorR,const I*actorT,const I*actorPosition,U mirror,I*out){
 I err=rtg_validate(d,len);if(err)return err;
 U nb=u(d+12),bo=u(d+28);
 if(!bones||boneFloats<nb*12||!actorR||!actorT||!actorPosition||!out||mirror>1)return 20;
 if(part==6)part=5;if(part==11)part=10;if(part!=5&&part!=10)return 21;
 I bi=-1;for(U i=0;i<nb;i++)if(si(d+bo+i*196+4)==(I)part){if(bi>=0)return 22;bi=(I)i;}
 if(bi<0)return 22;
 const float*p=bones+bi*12;for(U k=0;k<12;k++)if(!finite(p[k]))return 23;
 I result[3];
 for(U r=0;r<3;r++){
  double rotated=0;for(U k=0;k<3;k++)rotated+=double(actorR[r*3+k])*(mirror&&k==0?-1.:1.)*double(p[k*4+3]);
  double q=double(actorPosition[r])+(rotated+double(actorT[r])*4096.-double(actorPosition[r]))/16.;
  if(!(q>-2147483647.0&&q<2147483646.0))return 24;
  result[r]=(I)(q>=0?q+0.5:q-0.5);
 }
 for(U k=0;k<3;k++)out[k]=result[k];return 0;
}
// Warp only the six projection-input coordinates. Never edit native rope points,
// the controller, cached animation, actor movement or the environment anchor.
// The anchor has weight zero; the last point has weight one. Native waviness is
// retained, but even the last point's native flutter is pinned to the wrist.
API I rtg_web_segment(const I*start,const I*end,const I*nativeTip,const I*target,
 U index,U count,I*out){
 if(!start||!end||!nativeTip||!target||!out||!count||count>4096||index>=count)return 1;
 I temp[6];
 for(U v=0;v<2;v++)for(U k=0;k<3;k++){
  long long delta=(long long)target[k]-nativeTip[k];
  long long num=delta*(index+v),den=count;
  long long shift=num>=0?(num+den/2)/den:-((-num+den/2)/den);
  long long q=(long long)(v?end[k]:start[k])+shift;
  if(q<(-2147483647LL-1)||q>2147483647LL)return 2;
  temp[v*3+k]=(I)q;
 }
 for(U k=0;k<6;k++)out[k]=temp[k];return 0;
}
