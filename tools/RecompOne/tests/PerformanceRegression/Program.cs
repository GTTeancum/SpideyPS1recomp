using RecompOne.Runtime;
using RecompOne.Runtime.Assets;
using RecompOne.Runtime.Assets.Textures;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;
using RecompOne.Runtime.Hle;
using Silk.NET.OpenGL;
using Silk.NET.Windowing;
using Silk.NET.Maths;
using RecompOne.Runtime.Diagnostics;

void Check(bool ok, string label) { if (!ok) throw new Exception(label); Console.WriteLine("PASS " + label); }
if (args.Contains("--profile"))
{
    Check(PresentationProfile.Enabled, "profile tests require RECOMP_PERF_PHASES=1");
    PresentationProfile.EndPresentation();
    PresentationProfile.EventPumpMs = 75;
    PresentationProfile.InputPollMs = 12;
    PresentationProfile.ControllerEventsMs = 10;
    PresentationProfile.BeginPresentation();
    PresentationProfile.EventPumpMs = 2;
    Check(PresentationProfile.BetweenPresentations() is { EventPumpMs: 75, InputPollMs: 12, ControllerEventsMs: 10 }
        && PresentationProfile.Snapshot() is { EventPumpMs: 2 },
        "outside-present event stall remains separate from current presentation");
    PresentationProfile.EndPresentation();
    PresentationProfile.BeginPresentation();
    Check(PresentationProfile.BetweenPresentations() is { EventPumpMs: 0, InputPollMs: 0, ControllerEventsMs: 0 },
        "completed presentation event cost is not carried into the next interval");
    PresentationProfile.EndPresentation();
    long unit = System.Diagnostics.Stopwatch.Frequency;
    PresentationProfile.BeginPresentation();
    PresentationProfile.EnterRenderCallback(unit * 2);
    PresentationProfile.ExitRenderCallback(unit * 3);
    PresentationProfile.FinishRenderDispatch(unit, unit * 5);
    Check(PresentationProfile.Snapshot() is { RenderCallbacks: 1, BeforeRenderCallbackMs: 1000,
        AfterRenderCallbackMs: 2000, RenderDispatchMs: 4000 },
        "wrapper delays before and after callback remain distinct");
    PresentationProfile.EndPresentation();
    PresentationProfile.BeginPresentation();
    PresentationProfile.FinishRenderDispatch(unit, unit * 2);
    Check(PresentationProfile.Snapshot() is { RenderCallbacks: 0, BeforeRenderCallbackMs: null,
        AfterRenderCallbackMs: null }, "dispatch without callback has unavailable callback boundaries");
    PresentationProfile.EndPresentation();
}
var memory = new PSMemory(MemoryMap.DevkitRamSize);
var cpu = new CpuContext();
Runtime.SetContext(cpu, memory);
foreach (uint address in new uint[] { 0x80000ffc, 0x80001000, 0x807ffffc, 0x1f800000 })
{
    var tag = new GteScreen.VertexTag(123, 4.25f, 5.5f, true);
    GteScreen.StoreU32(memory, address, 0x12345678, tag);
    Check(memory.TryGetGteVertex(address, 0x12345678, out var found) && found == tag, $"tag survives page/scratch boundary {address:X}");
    memory.WriteU8(address + 1, 0x56); // even a same-value write invalidates provenance
    Check(!memory.TryGetGteVertex(address, 0x12345678, out _), "partial same-value write invalidates tag");
}
foreach (uint table in new uint[] { 0x800A0904, 0x800ACED8 })
{
    uint model = 0x80300000, metadata = 0x80301000, descriptor = 0x80302000;
    var texture = new ReplacementTexture { Width = 16, Height = 16 };
    var textures = new Dictionary<uint, ReplacementTexture> { [42] = texture };
    memory.WriteU32(table, 0x64697073); memory.WriteU16(table + 4, 0x7965); memory.WriteU8(table + 6, 0);
    memory.WriteU32(table + 20, model); memory.WriteU32(table + 12, metadata);
    memory.WriteU32(model + 8, 0); memory.WriteU32(model + 12, 0);
    memory.WriteU32(metadata, 1); memory.WriteU32(metadata + 4, descriptor);
    memory.WriteU8(descriptor, 32); memory.WriteU8(descriptor + 1, 32);
    memory.WriteU8(descriptor + 4, 47); memory.WriteU8(descriptor + 9, 47);
    memory.WriteU16(descriptor + 6, 3); memory.WriteU16(descriptor + 2, 64); memory.WriteU32(descriptor + 20, 42);
    var cache = new ActorMaterialCache(table);
    var tile = TextureTile.Describe(3, 64, 32, 32, 16, 16);
    Check(cache.Resolve(memory, textures, tile).Texture == texture, $"material resolves from game table {table:X}");
    Check(!cache.Resolve(memory, textures, TextureTile.Describe(3, 65, 32, 32, 16, 16)).Hit, "palette remains part of material identity");
    memory.WriteU16(descriptor + 6, 4); LibGpu.OtCount++;
    Check(!cache.Resolve(memory, textures, tile).Hit && cache.Resolve(memory, textures, TextureTile.Describe(4,64,32,32,16,16)).Hit,
        "new ordering table observes relocated material");
    memory.WriteU32(table + 20, 0); cache.Invalidate();
    Check(!cache.Resolve(memory, textures, TextureTile.Describe(4,64,32,32,16,16)).Hit, "released model invalidates binding");
}

if (args.Contains("--gl")) TestGl();
unsafe void TestGl()
{
    var options = WindowOptions.Default;
    options.IsVisible = false; options.Size = new Vector2D<int>(96, 1);
    options.API = new GraphicsAPI(ContextAPI.OpenGL, ContextProfile.Core, ContextFlags.Default, new APIVersion(3,3));
    using var window = Window.Create(options); window.Initialize();
    using var gl = GL.GetApi(window);
    uint Compile(ShaderType kind, string source)
    {
        uint shader = gl.CreateShader(kind); gl.ShaderSource(shader, source); gl.CompileShader(shader);
        gl.GetShader(shader, ShaderParameterName.CompileStatus, out int ok);
        Check(ok != 0, gl.GetShaderInfoLog(shader)); return shader;
    }
    uint vs = Compile(ShaderType.VertexShader, "#version 330 core\nlayout(location=0) in vec2 p; layout(location=1) in vec3 c; out vec3 color; void main(){ gl_Position=vec4(p,0,1); color=c; }");
    uint fs = Compile(ShaderType.FragmentShader, "#version 330 core\nin vec3 color; out vec4 result; void main(){ result=vec4(color,1); }");
    uint program = gl.CreateProgram(); gl.AttachShader(program, vs); gl.AttachShader(program, fs); gl.LinkProgram(program);
    gl.GetProgram(program, ProgramPropertyARB.LinkStatus, out int linked); Check(linked != 0, "GL shader linked");
    gl.UseProgram(program);
    uint texture = gl.GenTexture(); gl.BindTexture(TextureTarget.Texture2D, texture);
    gl.TexImage2D(TextureTarget.Texture2D, 0, InternalFormat.Rgba8, 96, 1, 0, PixelFormat.Rgba, PixelType.UnsignedByte, null);
    uint fbo = gl.GenFramebuffer(); gl.BindFramebuffer(FramebufferTarget.Framebuffer, fbo);
    gl.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, TextureTarget.Texture2D, texture, 0);
    uint vao = gl.GenVertexArray(); gl.BindVertexArray(vao);
    using var stream = new GlVertexStream(gl, 2, 20);
    gl.EnableVertexAttribArray(0); gl.VertexAttribPointer(0, 2, VertexAttribPointerType.Float, false, 20, (void*)0);
    gl.EnableVertexAttribArray(1); gl.VertexAttribPointer(1, 3, VertexAttribPointerType.Float, false, 20, (void*)8);
    gl.Viewport(0, 0, 96, 1); gl.Disable(EnableCap.Dither); gl.ClearColor(0,0,0,1); gl.Clear(ClearBufferMask.ColorBufferBit);
    var vertices = new TestVertex[1];
    for (int i = 0; i < 96; i++)
    {
        vertices[0] = new((i + .5f) / 48 - 1, 0, i % 3 == 0 ? 1 : 0, i % 3 == 1 ? 1 : 0, i % 3 == 2 ? 1 : 0);
        int first = stream.Upload<TestVertex>(vertices); gl.DrawArrays(PrimitiveType.Points, first, 1);
    }
    byte[] pixels = new byte[96 * 4];
    fixed(byte* data = pixels) gl.ReadPixels(0,0,96,1,PixelFormat.Rgba,PixelType.UnsignedByte,data);
    for (int i = 0; i < 96; i++)
        for(int channel = 0; channel < 3; channel++) Check(pixels[i*4+channel] == (i%3 == channel ? 255 : 0), $"stream wrap retains pixel {i}, channel {channel}");
    Check(gl.GetError() == GLEnum.NoError, "GL stream has no API errors");
    gl.DeleteFramebuffer(fbo); gl.DeleteTexture(texture); gl.DeleteVertexArray(vao); gl.DeleteProgram(program); gl.DeleteShader(vs); gl.DeleteShader(fs);
    GlVram.Scale = 1;
    GpuHle.WideAspect = 0;
    var renderer = new GlCore(gl, new Gl33Vram(gl));
    renderer.InitGl();
    GpuHle.NotifyDisplay(0, 0, 32, 32);
    var env = new HleDrawEnv { ClipX1 = 31, ClipY1 = 31 };
    renderer.SetDrawEnv(env);
    renderer.FillRect(0, 0, 32, 32, 0);
    void Rect(byte r, byte g, byte b, bool semi = false, ushort page = 0)
    {
        renderer.DrawRect(new HleRect { X = 4, Y = 4, W = 8, H = 8, R = r, G = g, B = b },
            new PrimFlags { SemiTrans = semi, TPage = page });
        renderer.Flush();
    }
    ushort Pixel()
    {
        Span<ushort> result = stackalloc ushort[1]; renderer.ReadVram(6, 6, 1, 1, result); return result[0];
    }
    Rect(255, 0, 0);
    Check((Pixel() & 0x7fff) == 31, "GL33 opaque target rendering without destination copy");
    Rect(0, 0, 255, true);
    ushort blended = Pixel();
    Check((blended & 31) is >= 14 and <= 17 && ((blended >> 10) & 31) is >= 14 and <= 17,
        "GL33 dual-source average preserves destination color");
    env.SetMask = true; renderer.SetDrawEnv(env); Rect(0,255,0);
    env.SetMask = false; env.CheckMask = true; renderer.SetDrawEnv(env); Rect(255,0,0);
    Check(Pixel() == (ushort)(0x8000 | (31 << 5)), "GL33 mask test still reads and protects destination");
    env.CheckMask = false; renderer.SetDrawEnv(env); Rect(255,255,255);
    Rect(64,64,64,true,64);
    ushort subtracted = Pixel();
    Check((subtracted & 31) is >= 22 and <= 24, "GL33 reverse-subtract blending remains correct");
    Check(gl.GetError() == GLEnum.NoError, "GL33 rendering has no API errors");
    renderer.Dispose();
}
readonly record struct TestVertex(float X, float Y, float R, float G, float B);
