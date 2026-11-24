using System.Numerics;
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
foreach(var level in visibility.RootElement.GetProperty("levels").EnumerateArray())
{
    string name=level.GetProperty("name").GetString()!;
    for(uint i=0;i<name.Length;i++) mem.WriteU8(0x800A568C+i,(byte)name[(int)i]);
    Check(N64ShadowVisibility.Level(mem)==(level.GetProperty("index").GetInt32(),false),"ROM level table: "+name);
}
mem.LoadBytes(0x800A568C, "dem1_t\0"u8.ToArray());
Check(N64ShadowVisibility.Level(mem).Demo, "demo filename selects demo policy");
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
