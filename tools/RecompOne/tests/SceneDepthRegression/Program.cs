using System.Buffers.Binary;
using Recompiled;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;
using Silk.NET.Maths;
using Silk.NET.OpenGL;
using Silk.NET.Windowing;

int checks = 0;
void Check(bool value, string name) { if (!value) throw new Exception(name); checks++; Console.WriteLine("PASS: " + name); }
var m = new PSMemory(0x800000);
var exe = File.ReadAllBytes(Path.Combine(Path.GetFullPath(args[0]), "spiderman/extracted/SLUS_008.75"));
m.LoadBytes(BinaryPrimitives.ReadUInt32LittleEndian(exe.AsSpan(0x18)), exe.AsSpan(2048).ToArray());
var cpu = new CpuContext { GP = 0x800B47F4, SP = 0x807EF000 };
const uint vertices = 0x80300000, output = 0x800A625C;
m.WriteU32(0x800B5918, 0x80310000);
m.WriteU32(0x800B58F0, output);
m.WriteU32(0x800B58F4, 0x800A819C);
m.WriteU32(0x800B5940, 0x800A7ED4);
m.WriteU16(vertices, 160); m.WriteU16(vertices + 2, 320); m.WriteU16(vertices + 4, 480); m.WriteU16(vertices + 6, 0);
SceneDepth.Install();
Check(GpuHle.SceneDepthEnabled, "SM1 enables normalized scene depth");
GteScreen.VertexTag Project(bool near, bool alternate = false)
{
    for (int i = 0; i < 5; i++) Gte.WriteControl(i, i is 0 or 2 or 4 ? 4096u : 0u);
    Gte.WriteControl(5, 0); Gte.WriteControl(6, 0); Gte.WriteControl(7, near ? 16000u : 1000u);
    Gte.WriteControl(24, 256u << 16); Gte.WriteControl(25, 120u << 16); Gte.WriteControl(26, 256);
    m.WriteU32(cpu.GP + 0x1170, near ? 1u : 0u);
    cpu.A0 = vertices; cpu.A1 = 1;
    if (alternate)
    {
        if (near) Recompiled.SpiderMan.func_8007BD04(cpu, m); else Recompiled.SpiderMan.func_8007BBD4(cpu, m);
    }
    else if (near) Recompiled.SpiderMan.func_8007B9CC(cpu, m); else Recompiled.SpiderMan.func_8007B798(cpu, m);
    Check(m.TryGetGteVertex(output, m.ReadU32(output), out var tag) && tag.HasSubpixel, "native actor transform carries validated projection");
    Check(Gte.Read(19) == (near ? 16480u : 1030u), "native SZ retains original precision units");
    Check(cpu.SP == 0x807EF000 && Gte.ProjectionDepthScale == 1f, "transform restores stack and host depth scope");
    return tag;
}
var far = Project(false); var near = Project(true);
Check(Math.Abs(far.Depth - 1030) < .001 && Math.Abs(near.Depth - far.Depth) < .001, "near/far native actor paths agree on scene depth (16x normalization)");
Check(Math.Abs(far.ScreenX - near.ScreenX) < .001 && Math.Abs(far.ScreenY - near.ScreenY) < .001, "normalization leaves exact projected geometry unchanged");
var again = Project(false);
Check(again.Depth == far.Depth, "following far actor cannot inherit previous near scale");
var alternateFar = Project(false, true); var alternateNear = Project(true, true);
Check(alternateFar.Depth == far.Depth && alternateNear.Depth == far.Depth,
    "both alternate native actor transforms also use consistent scene depth");
TestGl();
Console.WriteLine($"{checks} scene-depth checks passed; hidden GL context, no audio or host input.");

unsafe void TestGl()
{
    var options = WindowOptions.Default;
    options.IsVisible = false; options.Size = new Vector2D<int>(32, 32);
    options.API = new GraphicsAPI(ContextAPI.OpenGL, ContextProfile.Core, ContextFlags.Default, new APIVersion(3,3));
    using var window = Window.Create(options); window.Initialize();
    using var gl = GL.GetApi(window);
    GlVram.Scale = 1; GpuHle.WideAspect = 0;
    var renderer = new GlCore(gl, new Gl33Vram(gl));
    renderer.InitGl(); GpuHle.NotifyDisplay(0,0,32,32);
    renderer.SetDrawEnv(new HleDrawEnv { ClipX1 = 31, ClipY1 = 31 });
    void Clear() => renderer.FillRect(0,0,32,32,0);
    void Tri(float z, byte r, byte g, byte b, bool world = true, bool known = true, bool semi = false)
    {
        var a = new HleVertex { X=0,Y=0,Z=z,HasGteZ=known,R=r,G=g,B=b };
        var c = a; c.Y=32; var d = a; d.X=32;
        renderer.DrawTri(a,c,d,new PrimFlags { World=world,SemiTrans=semi }); renderer.Flush();
    }
    ushort Pixel() { Span<ushort> value=stackalloc ushort[1]; renderer.ReadVram(6,6,1,1,value); return (ushort)(value[0]&0x7fff); }
    Clear(); GpuHle.SceneDepthEnabled=false;
    Tri(500,255,0,0); Tri(1000,0,255,0);
    Check(Pixel()==(31<<5), "baseline reproduces actor painted through nearer wall");
    GpuHle.SceneDepthEnabled=true; Clear();
    Tri(500,255,0,0); Tri(1000,0,255,0);
    Check(Pixel()==31, "nearer opaque wall occludes later far actor");
    Clear(); Tri(1000,0,255,0); Tri(500,255,0,0);
    Check(Pixel()==31, "wall and actor visibility independent of submission order");
    Clear(); Tri(500,255,0,0); Tri(250,0,0,255);
    Check(Pixel()==(31<<10), "actor in front of wall remains visible");
    Tri(5000,0,255,0,world:false);
    Check(Pixel()==(31<<5), "screen-space HUD keeps painter ordering");
    Clear(); Tri(500,255,0,0); Tri(5000,0,255,0,known:false);
    Check(Pixel()==(31<<5), "unknown projection is not assigned invented occlusion depth");
    Clear(); Tri(500,255,0,0); Tri(1000,0,0,255,semi:true);
    ushort blended=Pixel();
    Check((blended&31) is >=14 and <=17 && ((blended>>10)&31) is >=14 and <=17, "native transparent effects retain blend/order semantics");
    Clear(); Tri(1000,0,255,0);
    Check(Pixel()==(31<<5), "frame clear discards previous scene depth");
    Clear(); Tri(500,255,0,0); renderer.BeginReplay(); Tri(1000,0,255,0);
    Check(Pixel()==(31<<5), "interpolated replay discards previous scene depth");
    renderer.WriteVram(960,0,2,1,new ushort[] { 0,0x7fff });
    void TexturedWall(float u)
    {
        var a = new HleVertex { X=0,Y=0,Z=500,HasGteZ=true,U=u,R=128,G=128,B=128 };
        var b=a; b.X=32; var c=a; c.Y=32;
        renderer.DrawTri(a,b,c,new PrimFlags { World=true,Textured=true,RawTexture=true,TPage=0x10f });
        renderer.Flush();
    }
    Clear(); TexturedWall(0); Tri(1000,0,255,0);
    Check(Pixel()==(31<<5), "transparent texture cut-out does not occlude geometry behind it");
    Clear(); TexturedWall(1); Tri(1000,0,255,0);
    ushort opaque = Pixel();
    // Native 5-bit white expands to 248, then the VRAM readback quantizes to 30.
    Check(opaque==0x7bde, $"opaque texture pixel writes scene depth and occludes far actor ({opaque:X4})");
    var replay = new FrameInterpolator(renderer);
    replay.SetDrawEnv(new HleDrawEnv { ClipX1=31, ClipY1=31 });
    replay.FillRect(0,0,32,32,0);
    foreach (var (z, red, green) in new[] { (500f,(byte)255,(byte)0), (1000f,(byte)0,(byte)255) })
    {
        var a = new HleVertex { X=0,Y=0,Z=z,HasGteZ=true,R=red,G=green };
        var b=a; b.X=32; var c=a; c.Y=32;
        replay.DrawTri(a,b,c,new PrimFlags { World=true });
    }
    replay.EndFrame();
    Check(replay.ReplaySafe && replay.TryGetReplayArea(out var area) &&
        area.ClipX0==0 && area.ClipY0==0 && area.ClipX1==31 && area.ClipY1==31,
        "capture locates recorded world buffer independently of display flip");
    GpuHle.SceneDepthEnabled=false; replay.Replay(1);
    Check(Pixel()==(31<<5), "same-frame replay with depth off reproduces overdraw");
    GpuHle.SceneDepthEnabled=true; replay.Replay(1);
    Check(Pixel()==31, "same-frame replay with depth on restores opaque occlusion");
    Check(gl.GetError()==GLEnum.NoError, "scene-depth rendering has no GL errors");
    renderer.Dispose();
}
