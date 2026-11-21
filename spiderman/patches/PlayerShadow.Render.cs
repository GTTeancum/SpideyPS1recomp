using System;
using System.Collections.Generic;
using System.Numerics;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hle;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>N64 ground silhouette, using the matching native ground collision result.</summary>
public static partial class PlayerShadow
{
    static readonly string Mode = Environment.GetEnvironmentVariable("SPIDEY_SHADOW")?.Trim().ToLowerInvariant() ?? "on";
    static readonly bool Trace = Environment.GetEnvironmentVariable("SPIDEY_TRACE_SHADOW") == "1";
    static readonly List<Vector3> Triangles = new(), _current = new();
    static readonly Dictionary<int, Vector3> _sources = new();
    static uint _receiverOwner;
    static uint _receiverFace;
    static bool _hasReceiver, _drawing;
    static long _nextTrace;
    static bool _traceDraw;
    static float _height, _h, _ofx, _ofy;
    static Vector3 _normal;
    static Matrix4x4 _view, _inverseView, _projection;

    public static void Install()
    {
        if (Mode != "off") Console.WriteLine("[shadow] N64 collision-plane silhouette (USA 800378B0)");
    }

    public static void Reset(CpuContext c, IMemory m)
    {
        _hasReceiver = _drawing = false; _receiverOwner = 0;
        _receiverFace = 0;
        ResetScript();
        Triangles.Clear(); _sources.Clear(); GpuPrims.Clear();
    }

    // The hook after the native ground query caches only the world-down
    // ground probe. On wall/ceiling ticks which skip this probe, N64 retains it.
    // A probe which misses explicitly invalidates it; never invent a floor.
    public static void GroundCollision(CpuContext c, IMemory m)
    {
        if (Mode == "off" || c.S3 == 0 || c.S3 != m.ReadU32(PlayerPointer)) return;
        _receiverOwner = c.S3;
        _hasReceiver = m.ReadU32(c.SP + GroundQueryOffset + 0x68) != 0;
        if (!_hasReceiver) return;
        _height = (int)m.ReadU32(c.SP + GroundQueryOffset + 0x70) / 4096f;
        _normal = new Vector3((short)m.ReadU16(c.SP + GroundQueryOffset + 0x78), (short)m.ReadU16(c.SP + GroundQueryOffset + 0x7A), (short)m.ReadU16(c.SP + GroundQueryOffset + 0x7C)) / 4096f;
        _receiverFace = m.ReadU32(c.SP + GroundQueryOffset + 0x80);
    }

    // Query with the original PS1 collision engine and a separate CPU context.
    // The hook runs before actor setup, so no live projected actor vertices exist.
    // Keep the caller's registers and GTE controls intact, and use unused stack
    // space below its SP instead of an allocation in the game's heap.
    static bool Probe(CpuContext caller, IMemory m, Vector3 start, Vector3 end,
        out float height, out Vector3 normal, out uint face)
    {
        uint query=caller.SP-0x100;
        Span<uint> controls=stackalloc uint[32];
        for (int i=0;i<32;i++) controls[i]=Gte.ReadControl(i);
        try
        {
            for (uint i=0;i<0x90;i+=4) m.WriteU32(query+i,0);
            for (int i=0;i<3;i++)
            {
                m.WriteU32(query+(uint)i*4,unchecked((uint)(int)(start[i]*4096)));
                m.WriteU32(query+12+(uint)i*4,unchecked((uint)(int)(end[i]*4096)));
            }
            var cpu=new CpuContext { SP=caller.SP-0x400, GP=caller.GP, A0=query };
            InitRay(cpu,m);
            cpu.A0=query;cpu.A1=1;
            CastRay(cpu,m);
            height=(int)m.ReadU32(query+0x70)/4096f;
            normal=new Vector3((short)m.ReadU16(query+0x78),(short)m.ReadU16(query+0x7A),(short)m.ReadU16(query+0x7C))/4096f;
            face=m.ReadU32(query+0x80);
            return m.ReadU32(query+0x68)!=0;
        }
        finally { for (int i=0;i<32;i++) Gte.WriteControl(i,controls[i]); }
    }

    public static (bool Material, uint Flags) ReceiverMaterial(IMemory m, uint face)
    {
        if (face<0x80000000 || face>=0x807FFFF0) return (false,0);
        uint flags=m.ReadU16(face);
        // PS1 8007C7A0 chooses a texture with face bit 0. 8007F760 shifts
        // face bit 6 into GP0's semi-transparency bit. N64 material +60 bit 0
        // selects the translucent RDP path at 800D4D90; it is not a body flag.
        return ((flags&1)!=0, (flags&0x40)!=0 ? 1u : 0u);
    }

    public static void ActorBegin(CpuContext c, IMemory m)
    {
        _drawing = false;
        if (Mode == "off" || c.A0 == 0 || c.A0 != m.ReadU32(PlayerPointer)) return;
        Triangles.Clear(); _sources.Clear();
        var scene = ShadowSceneForLevel(m);
        Vector3 player=new((int)m.ReadU32(c.A0+4)/4096f,(int)m.ReadU32(c.A0+8)/4096f,(int)m.ReadU32(c.A0+12)/4096f);
        float height=_height; Vector3 normal=_normal; uint face=_receiverFace;
        bool receiver=_hasReceiver && _receiverOwner==c.A0;
        if ((scene == ShadowScene.ZipLineTraining))
            receiver=Probe(c,m,player,player+new Vector3(0,8000,0),out height,out normal,out face);
        uint state=m.ReadU32(c.A0+PlayerStateOffset);
        bool attached=m.ReadU32(c.A0+AttachmentOffset)!=0;
        var (material,materialFlags)=ReceiverMaterial(m,face);
        if (!ShadowVisibility.AcceptsReceiver(scene, state, attached, ScriptHidden, receiver, material, materialFlags))
        {
            if (Trace) TraceHidden($"scene={scene} receiver={receiver} face={face:X8} material={material} flags={materialFlags} state={state:X} attached={attached} script={ScriptHidden}");
            return;
        }
        uint camera = m.ReadU32(c.GP + CameraOffset);
        if (camera < 0x80000000 || camera >= 0x807FFF00) return;
        uint matrix = ViewRotation(camera);
        Span<float> r = stackalloc float[9];
        for (uint i = 0; i < 9; i++) r[(int)i] = (short)m.ReadU16(matrix + i * 2) / 4096f;
        Vector3 eye = new((int)m.ReadU32(camera + 4), (int)m.ReadU32(camera + 8), (int)m.ReadU32(camera + 12));
        bool pixelDepth=ShadowVisibility.InitialPixelDepth(scene, state, eye, player);
        // The camera ray only affects visibility when the renderer's below-floor
        // early-out could apply. In all other cases both RDP modes draw the same
        // silhouette, and the host's scene-depth stencil already replaces them.
        if (ShadowVisibility.CameraHides(material, pixelDepth, eye.Y, r[7], height))
            pixelDepth=Probe(c,m,eye,new Vector3(player.X,_height-10,player.Z),out _,out _,out _);
        if (ShadowVisibility.CameraHides(material, pixelDepth, eye.Y, r[7], height))
        {
            if (Trace) TraceHidden($"camera below receiver: eye={eye} forwardY={r[7]} height={height}");
            return;
        }
        _view = new Matrix4x4(r[0],r[3],r[6],0, r[1],r[4],r[7],0, r[2],r[5],r[8],0, 0,0,0,1);
        _view.Translation = -Vector3.TransformNormal(eye, _view);
        if (!Matrix4x4.Invert(_view, out _inverseView)) return;
        Vector3 anchor = new(player.X,height,player.Z);
        _projection = N64ShadowProjection.Create(anchor, normal);
        _h = (short)Gte.ReadControl(26);
        _ofx = (int)Gte.ReadControl(24) / 65536f;
        _ofy = (int)Gte.ReadControl(25) / 65536f;
        _drawing = true;
        _traceDraw = Trace && Diag.Frame >= _nextTrace;
        if (_traceDraw)
        {
            _nextTrace = Diag.Frame + 60;
            Console.WriteLine($"[shadow-plane] frame={Diag.Frame} anchor={anchor} normal={normal} playerY={player.Y} eye={eye} scene={scene} attached={attached} face={face:X8} long={(scene == ShadowScene.ZipLineTraining)}");
        }
    }

    static void TraceHidden(string reason)
    {
        if (!Trace || Diag.Frame<_nextTrace) return;
        _nextTrace=Diag.Frame+60;
        Console.WriteLine($"[shadow-hidden] frame={Diag.Frame} {reason}");
    }

    public static void MeshDraw(CpuContext c, IMemory m)
    {
        if (!_drawing || c.S2 != m.ReadU32(PlayerPointer)) return;

        uint mesh = c.A0;
        if (mesh < 0x80000000u || mesh >= 0x807FF000u) return;
        int nv = m.ReadU16(mesh + 2), nn = m.ReadU16(mesh + 4), nf = m.ReadU16(mesh + 6);
        if (nv == 0 || nv > 512 || nf > 1024 || nn != nv + nf) return;

        Span<float> rt = stackalloc float[9];
        for (int i = 0; i < 4; i++)
        {
            uint w = Gte.ReadControl(i);
            rt[i * 2] = unchecked((short)w) / 4096f;
            rt[i * 2 + 1] = unchecked((short)(w >> 16)) / 4096f;
        }
        rt[8] = unchecked((short)Gte.ReadControl(4)) / 4096f;
        var tr = new Vector3((int)Gte.ReadControl(5), (int)Gte.ReadControl(6), (int)Gte.ReadControl(7));

        // Joints. A model's shared vertices (pad flag 1) are numbered in order across its
        // parts, and a joint vertex (flag 2) names the one it repeats: once loaded its x
        // is 8 x that number (the file has the number in y). Record each shared vertex by
        // its number as its part is drawn; a later part's joint takes it from there.
        // Looking joints up by transform output address instead missed most of them and
        // left holes at every joint (shoulders, hips, neck).
        int source = FirstSource(m, c.S2, mesh);
        // In near mode the native renderer uses 16x camera units.
        int shift = m.ReadU32(c.GP + PrecisionOffset) == 0 ? 4 : 0;
        _current.Clear();
        for (int i = 0; i < nv; i++)
        {
            uint v = mesh + 28 + (uint)i * 8;
            short x = unchecked((short)m.ReadU16(v)), y = unchecked((short)m.ReadU16(v + 2)), z = unchecked((short)m.ReadU16(v + 4));
            uint kind = m.ReadU16(v + 6) & 3u;
            Vector3 p;
            if ((kind & 2) != 0)
                p = _sources.TryGetValue((ushort)x / 8, out var joint) ? joint : new Vector3(float.NaN);
            else
            {
                int sx = x >> shift, sy = y >> shift, sz = z >> shift;
                p = tr + new Vector3(rt[0] * sx + rt[1] * sy + rt[2] * sz, rt[3] * sx + rt[4] * sy + rt[5] * sz, rt[6] * sx + rt[7] * sy + rt[8] * sz);
                // Near transforms use 16x units. Recover world coordinates before
                // applying the N64 projection; camera rotation cannot change the floor.
                if (shift == 0) p /= 16f;
                p = Vector3.Transform(p, _inverseView);
                if (kind == 1 && source >= 0) _sources[source++] = p;
            }
            _current.Add(p);
        }
        // Faces are variable length (u16 size at +2); vertex indices at +4..+7, the
        // fourth repeating the third for a triangle.
        uint face = mesh + 28 + (uint)(nv + nn) * 8;
        Span<int> idx = stackalloc int[4];
        for (int f = 0; f < nf; f++)
        {
            uint size = m.ReadU16(face + 2);
            if (size < 12 || size > 64) return;
            for (int k = 0; k < 4; k++) idx[k] = m.ReadU8(face + 4 + (uint)k);
            if (idx[0] < nv && idx[1] < nv && idx[2] < nv)
            {
                AddTriangle(idx[0], idx[1], idx[2]);
                if (idx[3] < nv && idx[3] != idx[2]) AddTriangle(idx[1], idx[3], idx[2]);
            }
            face += size;
        }
    }

    /// <summary>Shared vertices before part <paramref name="mesh"/> in the actor's model.</summary>
    static int FirstSource(IMemory m, uint actor, uint mesh)
    {
        uint slot = m.ReadU8(actor + 0x1B);
        if (slot >= 40) return -1;
        uint table = m.ReadU32(ModelTable + slot * 64u + 0x10u);
        if (table < 0x80000004u || table >= 0x80800000u) return -1;
        uint parts = m.ReadU32(table - 4);
        if (parts > 64) return -1;
        int count = 0;
        for (uint i = 0; i < parts; i++)
        {
            uint part = m.ReadU32(table + i * 4);
            if (part == mesh) return count;
            uint nv = m.ReadU16(part + 2);
            for (uint v = 0; v < nv && nv <= 512; v++)
                if ((m.ReadU16(part + 28 + v * 8 + 6) & 3u) == 1) count++;
        }
        return -1;
    }

    static void AddTriangle(int a, int b, int c)
    {
        Vector3 pa = _current[a], pb = _current[b], pc = _current[c];
        if (float.IsNaN(pa.X) || float.IsNaN(pb.X) || float.IsNaN(pc.X)) return;
        Triangles.Add(pa); Triangles.Add(pb); Triangles.Add(pc);
    }

    public static void ActorEnd(CpuContext c, IMemory m)
    {
        if (!_drawing) return;
        _drawing = false;
        uint env = m.ReadU32(c.GP + FrameOffset);
        if (_traceDraw)
            Console.WriteLine($"[shadow-mesh] frame={Diag.Frame} vertices={Triangles.Count} env={env:X8} first={(Triangles.Count > 0 ? Triangles[0] : Vector3.Zero)}");
        if (env < 0x80000000 || env >= 0x807FFF00) return;
        uint ot = m.ReadU32(env + 0x70);
        uint packet = m.ReadU32(c.GP + CursorOffset) & 0x7FFFFFFF;
        uint packetEnd = PacketLimit(c, m) & 0x7FFFFFFF;
        if (packet == 0 || packet + 8 >= packetEnd || packetEnd > 0x800000) return;
        GpuPrims.SetOrderingTable(ot, 4096);
        bool debug = Mode == "debug";
        var flags = new PrimFlags { World = true, Shadow = !debug, SemiTrans = !debug, TPage = 0 };
        Span<Vector3> input = stackalloc Vector3[3];
        Span<Vector3> clipped = stackalloc Vector3[4];
        Span<HleVertex> output = stackalloc HleVertex[4];
        int emitted = 0;
        for (int t = 0; t + 2 < Triangles.Count; t += 3)
        {
            for (int j = 0; j < 3; j++)
            {
                Vector3 world = debug ? Triangles[t + j] : Vector3.Transform(Triangles[t + j], _projection);
                input[j] = Vector3.Transform(world, _view);
            }
            int count = ClipNear(input, clipped);
            for (int j = 0; j < count; j++)
            {
                Vector3 p = clipped[j];
                output[j] = new HleVertex {
                    X = _h * p.X / p.Z * GpuHle.FovNum / GpuHle.FovDen + _ofx,
                    Y = _h * p.Y / p.Z + _ofy, Z = p.Z, HasGteZ = true,
                    R = debug ? (byte)255 : (byte)0, B = debug ? (byte)255 : (byte)0,
                    Id = 0x5AD00001u + (uint)(t * 2 + j) * 2 };
            }
            // Finish opaque world depth before the silhouette, then let native OT0
            // HUD/menu packets paint over it. Geometry supplies Z; it never writes Z.
            for (int j = 1; j + 1 < count; j++)
            {
                GpuPrims.ProjectedTri(0, output[0], output[j], output[j + 1], flags);
                emitted++;
            }
        }
        if (emitted > 0)
        {
            // The native optimizer bypasses empty OT slots (including OT0). A real
            // one-word NOP packet keeps this insertion point in its compacted chain.
            // Fully clipped poses must leave neither a packet nor a stale binding.
            uint previous = m.ReadU32(ot);
            m.WriteU32(packet, 0x01000000 | (previous & 0xFFFFFF));
            m.WriteU32(packet + 4, 0);
            m.WriteU32(ot, (previous & 0xFF000000) | packet);
            m.WriteU32(c.GP + CursorOffset, packet + 8);
            GpuPrims.AttachPacket(packet, 0);
        }
        if (_traceDraw)
            Console.WriteLine($"[shadow-queue] frame={Diag.Frame} triangles={emitted} ot={GpuPrims.OtBase:X8}");
    }

    public static int ClipNear(ReadOnlySpan<Vector3> input, Span<Vector3> output)
    {
        const float near = 1f;
        int count = 0;
        Vector3 previous = input[^1];
        foreach (Vector3 current in input)
        {
            if ((previous.Z >= near) != (current.Z >= near))
                output[count++] = Vector3.Lerp(previous, current, (near - previous.Z) / (current.Z - previous.Z));
            if (current.Z >= near) output[count++] = current;
            previous = current;
        }
        return count;
    }
}
