using System.Numerics;
using System.Reflection;
using System.Text.Json;
using Recompiled;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;
using Silk.NET.Maths;
using Silk.NET.OpenGL;
using Silk.NET.Windowing;

int checks = 0;
void Check(bool ok, string name) { if (!ok) throw new Exception(name); checks++; Console.WriteLine("PASS: " + name); }
string root = Path.GetFullPath(args[0]);
using var fixture = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, "tools/RecompOne/tests/ShadowRegression/n64-matrices.json")));
foreach (var item in fixture.RootElement.EnumerateArray())
{
    float[] n = item.GetProperty("normal").EnumerateArray().Select(x => x.GetSingle()).ToArray();
    float[] a = item.GetProperty("anchor").EnumerateArray().Select(x => x.GetSingle()).ToArray();
    float[] r = item.GetProperty("matrix").EnumerateArray().Select(x => x.GetSingle()).ToArray();
    var reference = new Matrix4x4(r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15]);
    var actual = N64ShadowProjection.Create(new(a[0],a[1],a[2]), new(n[0],n[1],n[2]));
    foreach (var v in new[] { Vector3.Zero, Vector3.One, new Vector3(15,-80,35), new Vector3(800,200,1400) })
        Check(Vector3.Distance(Vector3.Transform(v, reference), Vector3.Transform(v, actual)) < .001f, $"ROM matrix agrees for normal={string.Join(',',n)} vertex={v}");
}
var flat = N64ShadowProjection.Create(new(100,50,200), new(0,-1,0));
Check(Vector3.Transform(new(110,-900,220),flat)==new Vector3(110,50,220), "ceiling-height vertex flattens onto ground without diagonal light offset");
Span<Vector3> clipped = stackalloc Vector3[4];
Check(PlayerShadow.ClipNear(new Vector3[] {new(0,0,-2),new(0,4,4),new(4,0,4)},clipped)==4 && clipped[0].Z==1 && clipped[1].Z==1, "near-plane crossing clips to quad instead of dropping whole silhouette");
Check(PlayerShadow.ClipNear(new Vector3[] {new(0,0,-2),new(0,4,-1),new(4,0,0)},clipped)==0, "fully behind-camera silhouette is rejected");

using var visibility=JsonDocument.Parse(File.ReadAllText(Path.Combine(root,"tools/RecompOne/tests/ShadowRegression/n64-visibility.json")));
foreach (var item in visibility.RootElement.GetProperty("cases").EnumerateArray())
{
    var i=item.GetProperty("input");var o=item.GetProperty("output");
    int Int(string key)=>i.GetProperty(key).GetInt32();
    bool Bool(string key)=>i.GetProperty(key).GetBoolean();
    float Float(string key)=>i.GetProperty(key).GetSingle();
    Vector3 Vec(string key) { var a=i.GetProperty(key).EnumerateArray().Select(x=>x.GetSingle()).ToArray();return new(a[0],a[1],a[2]); }
    var eye=Vec("eye");var player=Vec("player");
    var scene=N64ShadowVisibility.Scene(Int("level"),Bool("demo"));
    bool mode=ShadowVisibility.InitialPixelDepth(scene,(uint)Int("state"),eye,player) || Bool("occluded");
    bool receiver=Bool("valid") && (!(scene==ShadowScene.ZipLineTraining) || Bool("long_valid"));
    bool draw=ShadowVisibility.AcceptsReceiver(scene,(uint)Int("state"),Bool("attached"),Bool("script_hidden"),
        receiver,Bool("material"),(uint)Int("material_flags")) && !ShadowVisibility.CameraHides(Bool("material"),mode,eye.Y,Float("forward_y"),Float("height"));
    Check(draw==o.GetProperty("draw").GetBoolean(),"ROM visibility: "+i.GetProperty("name").GetString());
    if (receiver && !Bool("material")) mode=true;
    Check(mode==(o.GetProperty("mode").GetInt32()!=0),"ROM depth mode: "+i.GetProperty("name").GetString());
    var rays=o.GetProperty("rays");
    Check(rays.EnumerateArray().Any(r=>r.GetProperty("kind").GetString()=="long")==(scene==ShadowScene.ZipLineTraining),"ROM training probe selection");
    foreach(var ray in rays.EnumerateArray())
    {
        float[] start=ray.GetProperty("start").EnumerateArray().Select(x=>x.GetSingle()).ToArray();
        float[] end=ray.GetProperty("end").EnumerateArray().Select(x=>x.GetSingle()).ToArray();
        Check(ray.GetProperty("kind").GetString()=="long" ? end[1]-start[1]==8000 : end[1]==Float("height")-10,"ROM ray endpoint");
    }
}
foreach(var s in visibility.RootElement.GetProperty("scripts").EnumerateArray())
    Check(N64ShadowVisibility.Script(s.GetProperty("before").GetBoolean(),s.GetProperty("level").GetInt32(),
        s.GetProperty("script").GetUInt32(),s.GetProperty("opcode").GetInt32(),s.GetProperty("operand").GetUInt32())==s.GetProperty("after").GetBoolean(),"ROM script suppression transition");

var mem = new PSMemory(0x800000);
const uint actor=0x80100000;
var cpu = new CpuContext { SP=0x807EF000, GP=0x800B47F4, S3=actor, A0=actor };
mem.WriteU32(0x800B5268,actor);
foreach(var level in visibility.RootElement.GetProperty("levels").EnumerateArray())
{
    string name=level.GetProperty("name").GetString()!;
    for(uint i=0;i<name.Length;i++) mem.WriteU8(0x800A568C+i,(byte)name[(int)i]);
    Check(N64ShadowVisibility.Level(mem)==(level.GetProperty("index").GetInt32(),false),"ROM level table: "+name);
}
void LevelName(string name) { for(uint i=0;i<16;i++) mem.WriteU8(0x800A568C+i,i<name.Length?(byte)name[(int)i]:(byte)0); }
LevelName("dem1_t");Check(N64ShadowVisibility.Level(mem).Demo,"demo filename selects demo policy, not a stale story index");
LevelName("l1a4_t");
mem.WriteU32(cpu.SP+0xD8,1); mem.WriteU32(cpu.SP+0xE0,200*4096); mem.WriteU16(cpu.SP+0xEA,unchecked((ushort)-4096));
PlayerShadow.GroundCollision(cpu,mem);
T Field<T>(string name) => (T)typeof(PlayerShadow).GetField(name,BindingFlags.Static|BindingFlags.NonPublic)!.GetValue(null)!;
Check(Field<bool>("_hasReceiver") && Field<float>("_height")==200 && Field<Vector3>("_normal")==new Vector3(0,-1,0), "native ground probe captures fixed-point receiver height/normal");
cpu.S3=actor+0x2000; mem.WriteU32(cpu.SP+0xD8,0); PlayerShadow.GroundCollision(cpu,mem);
Check(Field<bool>("_hasReceiver"), "NPC probe cannot replace player receiver");
cpu.S3=actor; PlayerShadow.GroundCollision(cpu,mem);
Check(!Field<bool>("_hasReceiver"), "player ground-probe miss invalidates cached receiver");
mem.WriteU32(cpu.SP+0xD8,1); PlayerShadow.GroundCollision(cpu,mem); PlayerShadow.Reset(cpu,mem);
Check(!Field<bool>("_hasReceiver") && Field<uint>("_receiverOwner")==0, "level reload clears old receiver even if actor address is reused");
cpu.A0=actor+0x2000;
Check(PlayerShadow.UpdateBlob(cpu,mem), "NPC blob stays under native ownership");
cpu.A0=actor; mem.WriteU16(actor+0x46,0x108);
Check(!PlayerShadow.UpdateBlob(cpu,mem) && mem.ReadU16(actor+0x46)==0x100 && cpu.SP==0x807EF000, "player blob suppressed without touching other body flags or stack");
cpu.A0=actor+0x2000;
Check(PlayerShadow.ShadowCheck(cpu,mem), "NPC shadow-check lifecycle is unchanged");
cpu.A0=actor; mem.WriteU8(actor+0x154,1);
Check(!PlayerShadow.ShadowCheck(cpu,mem) && mem.ReadU8(actor+0x154)==0, "player's separate PS1 shadow-check path cannot recreate its blob");
// Exercise actor hooks with an actual receiver and a pose fully behind the camera.
mem.WriteU32(cpu.SP+0xD8,1); PlayerShadow.GroundCollision(cpu,mem);
mem.WriteU32(cpu.GP+0x1128,0x80130000);
for (uint i=0;i<9;i++) mem.WriteU16(0x800A57CC+i*2, (ushort)(i%4==0?4096:0));
mem.WriteU32(cpu.GP+0xCB4,0x80140000); mem.WriteU32(0x80140070,0x80150000);
mem.WriteU32(cpu.GP+0xCBC,0x80160000); mem.WriteU32(0x800B4FE8,0x80170000);
mem.WriteU32(0x80150000,0x00FFFFFF);
PlayerShadow.ActorBegin(cpu,mem);
Field<List<Vector3>>("Triangles").AddRange(new Vector3[] {new(0,0,-20),new(2,0,-20),new(0,2,-20)});
PlayerShadow.ActorEnd(cpu,mem);
Check(!GpuPrims.Any && mem.ReadU32(cpu.GP+0xCBC)==0x80160000 && mem.ReadU32(0x80150000)==0x00FFFFFF,
    "fully clipped actor leaves native packet arena and OT untouched");
mem.WriteU32(actor+0xF44,0x400); PlayerShadow.ActorBegin(cpu,mem);
Check(Field<bool>("_drawing"), "unrelated F44 flag cannot hide shadow");
mem.WriteU32(actor+0xF3C,0x400); PlayerShadow.ActorBegin(cpu,mem);
Check(!Field<bool>("_drawing"), "N64 excluded player state 0x400 does not submit a shadow");
mem.WriteU32(actor+0xF3C,0);mem.WriteU32(actor+0xF44,0);
LevelName("l6a1_t");mem.WriteU32(actor+0xBC4,1);PlayerShadow.ActorBegin(cpu,mem);
Check(!Field<bool>("_drawing"),"Tunnel Crawl attached state BC4 suppresses silhouette");
LevelName("l6a3_t");PlayerShadow.ActorBegin(cpu,mem);
Check(!Field<bool>("_drawing"),"Lizard's Maze attached state BC4 suppresses silhouette");
LevelName("l1a4_t");PlayerShadow.ActorBegin(cpu,mem);
Check(Field<bool>("_drawing"),"bank ceiling attachment retains silhouette");
uint face=0x80131000;
mem.WriteU16(face,0x1F);Check(PlayerShadow.ReceiverMaterial(mem,face)==(true,0u),"textured opaque receiver");
mem.WriteU16(face,0x5F);Check(PlayerShadow.ReceiverMaterial(mem,face)==(true,1u),"PS1 semi-transparency maps to N64 material bit 0");
mem.WriteU16(face,0x1C);Check(PlayerShadow.ReceiverMaterial(mem,face)==(false,0u),"untextured receiver uses pixel depth");
cpu.S1=0x80132000;cpu.FP=0xEA;LevelName("l1a3_t");mem.WriteU16(cpu.S1,1);PlayerShadow.ScriptCinematic(cpu,mem);
Check(Field<bool>("_scriptHidden"),"native C3 hook sets L1A3 script EA suppression");
LevelName("l1a4_t");cpu.FP=0x16E;mem.WriteU16(cpu.S1,0);PlayerShadow.ScriptCinematic(cpu,mem);
Check(Field<bool>("_scriptHidden"),"bank 16E retains previous N64 cinematic value");
cpu.FP=1;PlayerShadow.ScriptCinematic(cpu,mem);
Check(!Field<bool>("_scriptHidden"),"native C3 cinematic end clears suppression");
LevelName("l1a3_t");cpu.FP=0xEA;mem.WriteU16(cpu.S1,1);PlayerShadow.ScriptCinematic(cpu,mem);
LevelName("l7a3_t");cpu.FP=0x15D;PlayerShadow.ScriptPlayerAction(cpu,mem);
Check(!Field<bool>("_scriptHidden"),"native C7 action clears L7A3 script 15D suppression");
LevelName("l1a4_t");mem.WriteU16(face,0x5F);mem.WriteU32(cpu.SP+0xF0,face);PlayerShadow.GroundCollision(cpu,mem);
PlayerShadow.ActorBegin(cpu,mem);Check(!Field<bool>("_drawing"),"native receiver face suppresses translucent material");
mem.WriteU16(face,0x1F);mem.WriteU32(0x80130008,300);mem.WriteU16(0x800A57CC+14,4096);
var beforeProbe=cpu.Snapshot();uint[] controls=Enumerable.Range(0,32).Select(Gte.ReadControl).ToArray();
PlayerShadow.ActorBegin(cpu,mem);
Check(!Field<bool>("_drawing"),"below-plane camera query with no occluder takes ROM early-out");
Check(cpu.Snapshot().gpr.SequenceEqual(beforeProbe.gpr) && Enumerable.Range(0,32).Select(Gte.ReadControl).SequenceEqual(controls),
    "native camera collision query preserves caller registers and GTE controls");
mem.WriteU32(0x80130008,200);PlayerShadow.ActorBegin(cpu,mem);
Check(Field<bool>("_drawing"),"camera exactly at receiving plane is not culled");
LevelName("lha1_t");PlayerShadow.ActorBegin(cpu,mem);
Check(!Field<bool>("_drawing"),"training's fresh native probe miss cannot use stale cached floor");
Check(cpu.Snapshot().gpr.SequenceEqual(beforeProbe.gpr) && Enumerable.Range(0,32).Select(Gte.ReadControl).SequenceEqual(controls),
    "native long collision query preserves caller registers and GTE controls");
PlayerShadow.Reset(cpu,mem);
TestGl();
Console.WriteLine($"{checks} shadow checks passed; hidden GL context, no audio or host input.");

unsafe void TestGl()
{
    var options=WindowOptions.Default; options.IsVisible=false; options.Size=new Vector2D<int>(32,32);
    options.API=new GraphicsAPI(ContextAPI.OpenGL,ContextProfile.Core,ContextFlags.Default,new APIVersion(3,3));
    using var window=Window.Create(options); window.Initialize(); using var gl=GL.GetApi(window);
    GlVram.Scale=1; GpuHle.WideAspect=0; GpuHle.SceneDepthEnabled=true;
    var renderer=new GlCore(gl,new Gl33Vram(gl)); renderer.InitGl(); GpuHle.NotifyDisplay(0,0,32,32);
    renderer.SetDrawEnv(new HleDrawEnv { ClipX1=31,ClipY1=31 });
    void Clear()=>renderer.FillRect(0,0,32,32,0);
    void Tri(float z,byte r,byte g,bool shadow=false,bool world=true)
    {
        var a=new HleVertex {X=0,Y=0,Z=z,HasGteZ=true,R=r,G=g};var b=a;var c=a;b.X=32;c.Y=32;
        renderer.DrawTri(a,b,c,new PrimFlags {World=world,Shadow=shadow,SemiTrans=shadow});renderer.Flush();
    }
    ushort Pixel() { Span<ushort> v=stackalloc ushort[1];renderer.ReadVram(6,6,1,1,v);return (ushort)(v[0]&0x7fff); }
    Clear();Tri(1000,255,0);Tri(1000,0,0,true);
    ushort half=Pixel();Check((half&31) is >=14 and <=16, "coplanar floor receives half-opacity shadow");
    Tri(1000,0,0,true);Check(Pixel()==half,"overlapping silhouette triangles darken a pixel once");
    Clear();Tri(400,255,0);Tri(1000,0,0,true);Check(Pixel()==31,"bank wall hides shadow behind it");
    Tri(400,0,0,true);Check(Pixel()==half,"occluded shadow does not poison stencil for a visible receiver");
    Clear();Tri(1000,255,0);Tri(500,0,0,true);Tri(750,0,255);
    Check(Pixel()==(31<<5),"shadow never writes depth or blocks later geometry");
    Clear();Tri(1000,255,0);Tri(1000,0,0,true);Tri(2000,0,255,world:false);
    Check(Pixel()==(31<<5),"native HUD remains above shadow and ignores scene depth");
    Clear();Tri(1000,255,0);Tri(1000,0,0,true);Check(Pixel()==half,"new frame clears previous silhouette stencil");
    var gpu = new Gpu(); Runtime.Gpu = gpu; GpuHle.Backend = renderer; GpuHle.Active = true;
    gpu.WriteGp0(0xE3000000); gpu.WriteGp0(0xE4000000 | 31 | (31u<<10)); gpu.WriteGp0(0xE5000000);
    var queueMemory = new PSMemory(0x200000);
    var queueCpu = new CpuContext { A0 = 0x80180000 };
    queueMemory.WriteU32(0x80180000,0x00FFFFFF); queueMemory.WriteU32(0x80190000,0x00FFFFFF);
    Clear(); Tri(1000,255,0);
    var qa = new HleVertex {X=0,Y=0,Z=1000,HasGteZ=true};var qb=qa;var qc=qa;qb.X=32;qc.Y=32;
    GpuPrims.SetOrderingTable(0x80180000,1);
    GpuPrims.ProjectedTri(0,qa,qb,qc,new PrimFlags {World=true,Shadow=true,SemiTrans=true});
    queueCpu.A0=0x80190000;RecompOne.Runtime.Sdk.LibGpu.DrawOTag(queueCpu,queueMemory);
    Check(GpuPrims.Any, "unrelated font/immediate OT cannot discard queued world silhouette");
    queueCpu.A0=0x80180000;RecompOne.Runtime.Sdk.LibGpu.DrawOTag(queueCpu,queueMemory);renderer.Flush();
    Check(!GpuPrims.Any && Pixel()==half,"native OT traversal emits projected shadow with depth and consumes queue");
    Clear(); Tri(1000,255,0);
    GpuPrims.SetOrderingTable(0x80180000,1);
    GpuPrims.ProjectedTri(0,qa,qb,qc,new PrimFlags {World=true,Shadow=true,SemiTrans=true});
    queueMemory.WriteU32(0x801A0000,0x01FFFFFF); queueMemory.WriteU32(0x801A0004,0);
    GpuPrims.AttachPacket(0x801A0000,0); queueCpu.A0=0x801A0000;
    RecompOne.Runtime.Sdk.LibGpu.DrawOTag(queueCpu,queueMemory);renderer.Flush();
    Check(!GpuPrims.Any && Pixel()==half,"compacted OT chain still emits bound silhouette packet without visiting OT0");
    GpuHle.Backend=null;Runtime.Gpu=null;
    Check(gl.GetError()==GLEnum.NoError,"shadow pipeline has no GL errors");renderer.Dispose();
}
