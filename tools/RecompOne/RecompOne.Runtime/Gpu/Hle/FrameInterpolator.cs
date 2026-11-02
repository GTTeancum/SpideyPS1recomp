namespace RecompOne.Runtime.Hle;

/// <summary>
/// 60 FPS presentation of a 30 FPS game without touching its clock.
///
/// Every backend call the game makes is passed straight through, so the frame it draws
/// is exactly what it always was, and is also recorded. When the frame is presented the
/// host keeps a copy of the real frame's image, replays the recording with each
/// projected vertex moved halfway back towards where the same vertex was in the previous
/// frame, presents that, and replays it unchanged to put the real frame back. The copy
/// is shown on the game's next console tick (HostWindow.PresentHeld); nothing waits, so
/// the ticks that drive the game clock are delivered exactly as without interpolation.
///
/// Scenery (every corner named by its world position, GteIds.CameraPosition) is redrawn
/// from a camera between the previous frame's and this one's, so finely and coarsely
/// split faces stay joined. Everything else is matched across frames by its GteIds name;
/// a vertex without one (HUD, screen-space effects) is drawn where the game put it. A
/// primitive only partly matched moves its unmatched corners with its matched ones, and
/// a point shared by several triangles moves the same way in all of them. A jump too
/// large for one frame is a camera cut, not motion.
///
///     RECOMP_INTERPOLATE=0/1   override the "60 FPS (interpolated)" setting
///     RECOMP_INTERPOLATE=mid   present only the in-between frames (capture checks)
///     RECOMP_INTERP_DEBUG=1    in-between frames show unnamed corners magenta and
///                              triangles with no match yellow
///     RECOMP_INTERP_TRACE=1    match counts and presentation spacing
///
/// VRAM transfers are not replayed: the live pass already left VRAM in its end state,
/// and replaying uploads would only rewind textures the next frame relies on.
/// </summary>
public sealed class FrameInterpolator : IGpuBackend
{
    enum Kind : byte { Env, Tri, Rect, Line, Fill }

    struct Op
    {
        public Kind Kind;
        public HleDrawEnv Env;
        public HleVertex A, B, C;
        public HleRect Rect;
        public PrimFlags Flags;
        public int X, Y, W, H;
        public ushort Color;
        public byte BgR, BgG, BgB, DrawBgR, DrawBgG, DrawBgB;
    }

    readonly IGpuBackend _inner;
    List<Op> _frame = new(32768), _last = new(32768);
    readonly Dictionary<uint, (float X, float Y, float Z)> _previous = new(32768);
    HleDrawEnv _env;
    Hardware.GteIds.Camera _camera, _priorCamera;
    readonly double[] _inverse = new double[9];
    bool _reproject;

    /// <summary>Largest per-frame movement, in console pixels, still treated as motion.</summary>
    public static float MaxStep = 96f;

    public static long Matched, Unmatched, Cuts, Frames;
    static readonly bool Trace = Environment.GetEnvironmentVariable("RECOMP_INTERP_TRACE") == "1";
    static readonly bool Debug = Environment.GetEnvironmentVariable("RECOMP_INTERP_DEBUG") == "1";

    public FrameInterpolator(IGpuBackend inner) { _inner = inner; }

    public bool Ready => _inner.Ready;
    public bool HasFrame => _last.Count > 0;

    public void SetDrawEnv(in HleDrawEnv env)
    {
        _env = env;
        _frame.Add(new Op { Kind = Kind.Env, Env = env });
        _inner.SetDrawEnv(env);
    }

    Op WithBackground(Op op, in PrimFlags f)
    {
        if (!f.Background) return op;
        op.BgR = GpuHle.BackgroundR; op.BgG = GpuHle.BackgroundG; op.BgB = GpuHle.BackgroundB;
        op.DrawBgR = GpuHle.DrawBackgroundR; op.DrawBgG = GpuHle.DrawBackgroundG; op.DrawBgB = GpuHle.DrawBackgroundB;
        return op;
    }

    public void DrawTri(in HleVertex a, in HleVertex b, in HleVertex c, in PrimFlags f)
    {
        _frame.Add(WithBackground(new Op { Kind = Kind.Tri, A = a, B = b, C = c, Flags = f }, f));
        _inner.DrawTri(a, b, c, f);
    }

    public void DrawRect(in HleRect r, in PrimFlags f)
    {
        _frame.Add(WithBackground(new Op { Kind = Kind.Rect, Rect = r, Flags = f }, f));
        _inner.DrawRect(r, f);
    }

    public void DrawLine(in HleVertex a, in HleVertex b, in PrimFlags f)
    {
        _frame.Add(new Op { Kind = Kind.Line, A = a, B = b, Flags = f });
        _inner.DrawLine(a, b, f);
    }

    public void FillRect(int x, int y, int w, int h, ushort color15)
    {
        _frame.Add(new Op { Kind = Kind.Fill, X = x, Y = y, W = w, H = h, Color = color15 });
        _inner.FillRect(x, y, w, h, color15);
    }

    public void CopyVram(int sx, int sy, int dx, int dy, int w, int h) => _inner.CopyVram(sx, sy, dx, dy, w, h);
    public void WriteVram(int x, int y, int w, int h, ReadOnlySpan<ushort> px) => _inner.WriteVram(x, y, w, h, px);
    public void ReadVram(int x, int y, int w, int h, Span<ushort> px) => _inner.ReadVram(x, y, w, h, px);
    public byte[]? ReadScaled(int x, int y, int w, int h, out int outW, out int outH) => _inner.ReadScaled(x, y, w, h, out outW, out outH);
    public byte[]? ReadPresented(out int outW, out int outH) => _inner.ReadPresented(out outW, out outH);
    public byte[]? ReadPreFxaa(out int outW, out int outH) => _inner.ReadPreFxaa(out outW, out outH);
    public int RegisterImage(ReadOnlySpan<byte> rgba, int width, int height) => _inner.RegisterImage(rgba, width, height);
    public void Flush() => _inner.Flush();
    public void Present(in HleDispEnv disp) => _inner.Present(disp);
    public void BeginReplay() => _inner.BeginReplay();

    /// <summary>
    /// Close the frame the game just finished: it becomes the one replayed by
    /// <see cref="Replay"/>, and the one before it supplies the earlier positions.
    /// Returns false when there is nothing to interpolate from.
    /// </summary>
    public bool EndFrame()
    {
        // No drawing since the last presentation: the game is running slower than
        // 30 FPS. Keep both frames; the screen already shows the latest one.
        if (_frame.Count == 0) return false;
        bool hadPrevious = _last.Count > 0;
        // Earlier positions come from the frame being retired.
        _previous.Clear();
        var env = default(HleDrawEnv);
        foreach (var op in _last)
        {
            if (op.Kind == Kind.Env) env = op.Env;
            else if (op.Kind == Kind.Tri) { Remember(op.A, env); Remember(op.B, env); Remember(op.C, env); }
        }
        (_last, _frame) = (_frame, _last);
        _frame.Clear();
        _priorCamera = _camera;
        _camera = Hardware.GteIds.FrameCamera;
        _reproject = _camera.Valid && _priorCamera.Valid && Invert(_camera.R, _inverse);
        Frames++;
        if (Trace && Frames % 60 == 0)
        {
            var origins = new HashSet<(int, int)>();
            foreach (var op in _last) if (op.Kind == Kind.Env) origins.Add((op.Env.ClipX0, op.Env.ClipY0));
            Console.WriteLine($"[interp] frame {Frames}: {_last.Count} ops, areas {string.Join(" ", origins)}, " +
                $"previous {_previous.Count} named; matched {Matched} unmatched {Unmatched} cuts {Cuts} named {Hardware.GteIds.Named} misses {Hardware.GteIds.NameMisses}");
        }
        return hadPrevious && _last.Count > 0;
    }

    void Remember(in HleVertex v, in HleDrawEnv env)
    {
        if (v.Id == 0) return;
        var at = (v.X - env.ClipX0, v.Y - env.ClipY0, v.Z);
        _previous.TryAdd(v.Id, at);
        if (v.AltId != 0) _previous.TryAdd(v.AltId, at);
    }

    bool TryPrevious(in HleVertex v, out (float X, float Y, float Z) p)
    {
        p = default;
        return v.Id != 0 && (_previous.TryGetValue(v.Id, out p) ||
            (v.AltId != 0 && _previous.TryGetValue(v.AltId, out p)));
    }

    /// <summary>
    /// Draw the closed frame again with every matched vertex at fraction
    /// <paramref name="t"/> of the way from its previous position to its current one
    /// (1 = the real frame).
    /// </summary>
    public void Replay(float t)
    {
        _inner.Flush();
        _inner.BeginReplay();
        bool cut = t < 1f && IsCut();
        if (t < 1f && !cut) CollectMoves();
        if (t < 1f && _reproject) MidCamera(t);
        HleDrawEnv env = default;
        Span<HleVertex> v = stackalloc HleVertex[3];
        foreach (var op in _last)
        {
            switch (op.Kind)
            {
                case Kind.Env:
                    env = op.Env;
                    _inner.SetDrawEnv(env);
                    break;
                case Kind.Fill:
                    _inner.FillRect(op.X, op.Y, op.W, op.H, op.Color);
                    break;
                case Kind.Rect:
                    SetBackground(op);
                    _inner.DrawRect(op.Rect, op.Flags);
                    break;
                case Kind.Line:
                    _inner.DrawLine(op.A, op.B, op.Flags);
                    break;
                case Kind.Tri:
                    v[0] = op.A; v[1] = op.B; v[2] = op.C;
                    if (t < 1f && !cut &&
                        !(IsScenery(v[0]) && IsScenery(v[1]) && IsScenery(v[2]) && Reproject(v, env)))
                        Interpolate(v, env, t);
                    SetBackground(op);
                    if (Debug && t < 1f)
                    {
                        // Unnamed corners magenta, named but unmatched yellow.
                        var f = op.Flags; f.RawTexture = false;
                        for (int i = 0; i < 3; i++)
                            if (v[i].Id == 0 && v[i].HasGteZ) { v[i].R = 0; v[i].G = 255; v[i].B = 255; }
                            else if (v[i].Id == 0) { v[i].R = 255; v[i].G = 0; v[i].B = 255; }
                            else if (!TryPrevious(op.A, out _) && !TryPrevious(op.B, out _) && !TryPrevious(op.C, out _)) { v[i].R = 255; v[i].G = 255; v[i].B = 0; }
                        _inner.DrawTri(v[0], v[1], v[2], f);
                        break;
                    }
                    _inner.DrawTri(v[0], v[1], v[2], op.Flags);
                    break;
            }
        }
        _inner.SetDrawEnv(_env);
        _inner.Flush();
    }

    static void SetBackground(in Op op)
    {
        if (!op.Flags.Background) return;
        GpuHle.BackgroundR = op.BgR; GpuHle.BackgroundG = op.BgG; GpuHle.BackgroundB = op.BgB;
        GpuHle.DrawBackgroundR = op.DrawBgR; GpuHle.DrawBackgroundG = op.DrawBgG; GpuHle.DrawBackgroundB = op.DrawBgB;
    }

    /// <summary>
    /// A camera cut moves almost everything a long way at once. Interpolating across it
    /// would show a frame of the scene smeared between two shots.
    /// </summary>
    bool IsCut()
    {
        int matched = 0, jumped = 0;
        var env = default(HleDrawEnv);
        foreach (var op in _last)
        {
            if (op.Kind == Kind.Env) { env = op.Env; continue; }
            if (op.Kind != Kind.Tri) continue;
            Count(op.A, env, ref matched, ref jumped);
        }
        if (matched == 0) return false;
        bool cut = jumped * 3 > matched;
        if (cut) Cuts++;
        return cut;
    }

    void Count(in HleVertex v, in HleDrawEnv env, ref int matched, ref int jumped)
    {
        if (!TryPrevious(v, out var p)) return;
        matched++;
        float dx = v.X - env.ClipX0 - p.X, dy = v.Y - env.ClipY0 - p.Y;
        if (dx * dx + dy * dy > MaxStep * MaxStep) jumped++;
    }

    static bool Invert(double[] m, double[] inv)
    {
        double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
        double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
        if (Math.Abs(det) < 1e-9) return false;
        inv[0] = (e * i - f * h) / det; inv[1] = (c * h - b * i) / det; inv[2] = (b * f - c * e) / det;
        inv[3] = (f * g - d * i) / det; inv[4] = (a * i - c * g) / det; inv[5] = (c * d - a * f) / det;
        inv[6] = (d * h - e * g) / det; inv[7] = (b * g - a * h) / det; inv[8] = (a * e - b * d) / det;
        return true;
    }

    public static long Reprojected;
    readonly double[] _midR = new double[9], _midC = new double[3];
    double _midH, _midOfX, _midOfY, _midSqueeze;

    /// <summary>The camera part of the way from the previous frame's to this one's.</summary>
    void MidCamera(float t)
    {
        var a = _priorCamera; var b = _camera;
        for (int i = 0; i < 9; i++) _midR[i] = a.R[i] + (b.R[i] - a.R[i]) * t;
        // Blending two rotations shortens the rows slightly. Restore each row to the
        // blend of its two lengths: the game's view matrix is not a pure rotation (its
        // rows carry the pixel-aspect scale), so it must not be normalised to unit length.
        for (int row = 0; row < 3; row++)
        {
            double la = 0, lb = 0, lm = 0;
            for (int k = 0; k < 3; k++)
            {
                int i = row * 3 + k;
                la += a.R[i] * a.R[i]; lb += b.R[i] * b.R[i]; lm += _midR[i] * _midR[i];
            }
            if (lm < 1e-12) { _reproject = false; return; }
            double scale = (Math.Sqrt(la) + (Math.Sqrt(lb) - Math.Sqrt(la)) * t) / Math.Sqrt(lm);
            for (int k = 0; k < 3; k++) _midR[row * 3 + k] *= scale;
        }
        for (int i = 0; i < 3; i++) _midC[i] = a.C[i] + (b.C[i] - a.C[i]) * t;
        _midH = a.H + (b.H - a.H) * t;
        _midOfX = a.OfX + (b.OfX - a.OfX) * t;
        _midOfY = a.OfY + (b.OfY - a.OfY) * t;
        _midSqueeze = a.Squeeze + (b.Squeeze - a.Squeeze) * t;
    }

    /// <summary>
    /// Scenery does not move by itself: undo this frame's camera and view it from the
    /// in-between camera. Every point is placed by the same rigid view, so edges stay
    /// straight and neighbouring faces, split finely or not, keep meeting exactly.
    /// </summary>
    bool Reproject(Span<HleVertex> v, in HleDrawEnv env)
    {
        if (!_reproject) return false;
        var now = _camera;
        Span<float> x2 = stackalloc float[3], y2 = stackalloc float[3], z2 = stackalloc float[3];
        for (int k = 0; k < 3; k++)
        {
            if (!v[k].HasGteZ || v[k].Z <= 1f) return false;
            double z = v[k].Z;
            double sx = v[k].X - env.ClipX0, sy = v[k].Y - env.ClipY0;
            double x = (sx - now.OfX) * z / (now.H * now.Squeeze), y = (sy - now.OfY) * z / now.H;
            double wx = _inverse[0] * x + _inverse[1] * y + _inverse[2] * z + now.C[0] - _midC[0];
            double wy = _inverse[3] * x + _inverse[4] * y + _inverse[5] * z + now.C[1] - _midC[1];
            double wz = _inverse[6] * x + _inverse[7] * y + _inverse[8] * z + now.C[2] - _midC[2];
            var r = _midR;
            double qx = r[0] * wx + r[1] * wy + r[2] * wz;
            double qy = r[3] * wx + r[4] * wy + r[5] * wz;
            double qz = r[6] * wx + r[7] * wy + r[8] * wz;
            if (qz <= 1) return false;
            double px = _midH * qx / qz * _midSqueeze + _midOfX, py = _midH * qy / qz + _midOfY;
            float mx = (float)(px - sx), my = (float)(py - sy);
            if (mx * mx + my * my > MaxStep * MaxStep) return false;
            x2[k] = (float)px + env.ClipX0; y2[k] = (float)py + env.ClipY0; z2[k] = (float)qz;
        }
        for (int k = 0; k < 3; k++) { v[k].X = x2[k]; v[k].Y = y2[k]; v[k].Z = MathF.Max(1f, z2[k]); }
        Reprojected++;
        return true;
    }

    /// <summary>Scenery: every corner named by its world position or by a subdivision.</summary>
    static bool IsScenery(in HleVertex v) => v.AltId != 0 || (v.Id & 0x80000000u) != 0;

    // Movement chosen for each on-screen point this replay, so every triangle that
    // shares a corner moves it the same way and adjacent faces cannot open a seam.
    readonly Dictionary<ulong, (float X, float Y, float Z)> _moves = new(32768);

    static ulong PointKey(in HleVertex v) =>
        (((ulong)BitConverter.SingleToUInt32Bits(v.X) << 32) | BitConverter.SingleToUInt32Bits(v.Y)) ^
        BitConverter.SingleToUInt32Bits(v.Z) * 0x9E3779B97F4A7C15ul;

    /// <summary>Every point seen last frame claims its movement before any guess is made.</summary>
    void CollectMoves()
    {
        _moves.Clear();
        var env = default(HleDrawEnv);
        foreach (var op in _last)
        {
            if (op.Kind == Kind.Env) { env = op.Env; continue; }
            if (op.Kind != Kind.Tri) continue;
            Claim(op.A, env); Claim(op.B, env); Claim(op.C, env);
        }
    }

    void Claim(in HleVertex v, in HleDrawEnv env)
    {
        if (!TryPrevious(v, out var p)) return;
        float mx = p.X + env.ClipX0 - v.X, my = p.Y + env.ClipY0 - v.Y;
        if (mx * mx + my * my > MaxStep * MaxStep) return;
        _moves.TryAdd(PointKey(v), (mx, my, p.Z - v.Z));
    }

    void Interpolate(Span<HleVertex> v, in HleDrawEnv env, float t)
    {
        Span<ulong> key = stackalloc ulong[3];
        Span<bool> has = stackalloc bool[3];
        Span<float> dx = stackalloc float[3], dy = stackalloc float[3], dz = stackalloc float[3];
        float sumX = 0, sumY = 0, sumZ = 0;
        int found = 0;
        for (int i = 0; i < 3; i++)
        {
            key[i] = PointKey(v[i]);
            has[i] = _moves.TryGetValue(key[i], out var m);
            if (!has[i]) continue;
            dx[i] = m.X; dy[i] = m.Y; dz[i] = m.Z;
            sumX += m.X; sumY += m.Y; sumZ += m.Z;
            found++;
        }
        if (found == 0) { Unmatched += 3; return; }
        Matched += found; Unmatched += 3 - found;
        for (int i = 0; i < 3; i++)
            if (!has[i]) { dx[i] = sumX / found; dy[i] = sumY / found; dz[i] = sumZ / found; }
        float back = 1f - t;
        for (int i = 0; i < 3; i++)
        {
            if (!has[i]) _moves.TryAdd(key[i], (dx[i], dy[i], dz[i]));
            v[i].X += dx[i] * back;
            v[i].Y += dy[i] * back;
            if (v[i].HasGteZ) v[i].Z = MathF.Max(1f, v[i].Z + dz[i] * back);
        }
    }
}
