using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Hle;

var memory = new PSMemory(0x800000);
var cpu = new CpuContext();
const uint source = 0x80010000, wholePacket = 0x80010100, splitPacket = 0x80010200;
int failures = 0;
foreach (int reg in new[] { 12, 13, 14, 15 })
{
    const uint word = 123u | (67u << 16);
    var tag = new GteScreen.VertexTag(2400, 123.75f, 67.25f, true);
    GteScreen.StoreU32(memory, source, word, tag);
    GteScreen.LoadU32(cpu, 25, memory, source);
    RecompOne.Runtime.Gte.Write(reg, cpu[25]);
    RecompOne.Runtime.Gte.ReadTo(cpu, 8, reg);
    var old = cpu.GetGteVertexTag(8);
    RecompOne.Runtime.Gte.WriteFrom(cpu, reg, 25);
    RecompOne.Runtime.Gte.ReadTo(cpu, 8, reg);
    var repaired = cpu.GetGteVertexTag(8);
    bool pass = cpu[8] == word && old.Depth == 0 && repaired == tag;
    Console.WriteLine($"MTC2 screen register{reg}: old-depth={old.Depth} repaired={repaired} {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
foreach (var (x, y) in new[] { (123.75f, 67.25f), (-23.125f, -7.625f) })
{
    uint packed = (ushort)(short)MathF.Floor(x) | (uint)(ushort)(short)MathF.Floor(y) << 16;
    var expected = new GteScreen.VertexTag(2500, x, y, true);
    GteScreen.StoreU32(memory, source, packed, expected);
    GteScreen.LoadU32(cpu, 8, memory, source);
    GteScreen.StoreU32(memory, wholePacket, cpu[8], cpu.GetGteVertexTag(8));
    GteScreen.LoadU16(cpu, 9, memory, source, false);
    GteScreen.LoadU16(cpu, 10, memory, source + 2, true);
    cpu.SetDerived(10, cpu[10] << 16, 10);
    cpu.SetDerived(9, cpu[9] | cpu[10], 9, 10);
    GteScreen.StoreU32(memory, splitPacket, cpu[9], cpu.GetGteVertexTag(9));
    memory.TryGetGteVertex(wholePacket, packed, out var whole);
    memory.TryGetGteVertex(splitPacket, packed, out var split);
    bool pass = whole == expected && split == expected;
    Console.WriteLine($"shared-edge whole/split x={x} y={y}: whole={whole} split={split} {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;

    // Engine stores outcodes beside SXY in scratch RAM before stripping them for GP0.
    uint encoded = packed | 0x40004000u;
    GteScreen.StoreU32(memory, source + 8, encoded, expected);
    GteScreen.LoadU32(cpu, 11, memory, source + 8);
    cpu.SetDerived(11, packed, 11);
    GteScreen.StoreU32(memory, splitPacket, cpu[11], cpu.GetGteVertexTag(11));
    memory.TryGetGteVertex(splitPacket, packed, out var decoded);
    Console.WriteLine($"scratch outcodes round-trip: {(decoded == expected ? "PASS" : "FAIL")}");
    if (decoded != expected) failures++;

    // A real coordinate edit must never inherit the original fractional location.
    cpu.SetDerived(9, cpu[9] + 1, 9);
    GteScreen.StoreU32(memory, splitPacket, cpu[9], cpu.GetGteVertexTag(9));
    memory.TryGetGteVertex(splitPacket, cpu[9], out var moved);
    moved = GteScreen.ValidatePacketVertex(cpu[9], moved);
    if (moved.HasSubpixel) { Console.WriteLine("edited coordinate retained stale projection: FAIL"); failures++; }
    memory.WriteU16(source, 0);
    GteScreen.LoadU16(cpu, 9, memory, source + 2, false);
    if (cpu.GetGteVertexTag(9).Depth != 0) { Console.WriteLine("partial overwrite retained stale tag: FAIL"); failures++; }
}
const uint edgeAddress = 0x1F800010;
const uint edgeWord = 30u | (40u << 16);
var edgeTag = new GteScreen.VertexTag(1200, 30.5f, 40.75f, true);
CpuContext EdgeCpu() => new() { A0 = 0x1F800000, A2 = 2, T6 = 16, T8 = 0, T9 = 0 };
GteScreen.StoreU32(memory, edgeAddress, edgeWord, edgeTag);
var nativeCpu = EdgeCpu();
NativeGame.EdgeNative(nativeCpu, memory);
uint nativeWord = memory.ReadU32(edgeAddress);
memory.TryGetGteVertex(edgeAddress, nativeWord, out var nativeTag);
bool nativeFractional = GteScreen.ValidatePacketVertex(nativeWord, nativeTag).HasSubpixel;
GteScreen.StoreU32(memory, edgeAddress, edgeWord, edgeTag);
var hookedCpu = EdgeCpu();
NativeGame.Edge(hookedCpu, memory);
uint hookedWord = memory.ReadU32(edgeAddress);
memory.TryGetGteVertex(edgeAddress, hookedWord, out var hookedTag);
var finalTag = GteScreen.ValidatePacketVertex(hookedWord, hookedTag);
bool edgePass = nativeWord == hookedWord && nativeWord == (31u | 39u << 16) &&
    nativeCpu.Snapshot().gpr.SequenceEqual(hookedCpu.Snapshot().gpr) &&
    !nativeFractional && finalTag.HasSubpixel && finalTag.ScreenX == 30.5f && finalTag.ScreenY == 40.75f;
Console.WriteLine($"Engine edge routine: native word=0x{nativeWord:X8} hooked=0x{hookedWord:X8} " +
    $"native subpixel={nativeFractional} hooked={finalTag} {(edgePass ? "PASS" : "FAIL")}");
if (!edgePass) failures++;
// Actual failing packet observed at the widescreen left edge: 0,130 -> -1,128.
// T9-T8=0x20000000 chooses the native packed delta -65537.
var borrowCpu = EdgeCpu();
borrowCpu.T9 = 0x20000000;
GteScreen.StoreU32(memory, edgeAddress, 130u << 16,
    new GteScreen.VertexTag(1155, 0.5391998f, 130.03665f, true));
NativeGame.Edge(borrowCpu, memory);
uint borrowWord = memory.ReadU32(edgeAddress);
memory.TryGetGteVertex(edgeAddress, borrowWord, out var borrowTag);
borrowTag = GteScreen.ValidatePacketVertex(borrowWord, borrowTag);
bool borrowPass = borrowWord == 0x0080FFFF && borrowTag.HasSubpixel &&
    Math.Abs(borrowTag.ScreenX - 0.5391998f) < 0.0001f &&
    Math.Abs(borrowTag.ScreenY - 130.03665f) < 0.0001f;
Console.WriteLine($"Engine packed edge borrow: word={borrowWord:X8} tag={borrowTag} {(borrowPass ? "PASS" : "FAIL")}");
if (!borrowPass) failures++;
// A ground vertex below the viewport must retain its true projection for GPU
// clipping/interpolation, while the emulated SXY register remains PS1-saturated.
RecompOne.Runtime.Gte.WriteControl(0, 4096);
RecompOne.Runtime.Gte.WriteControl(1, 0);
RecompOne.Runtime.Gte.WriteControl(2, 4096);
RecompOne.Runtime.Gte.WriteControl(3, 0);
RecompOne.Runtime.Gte.WriteControl(4, 4096);
for (int i = 5; i <= 7; i++) RecompOne.Runtime.Gte.WriteControl(i, 0);
RecompOne.Runtime.Gte.WriteControl(24, 160u << 16);
RecompOne.Runtime.Gte.WriteControl(25, 120u << 16);
RecompOne.Runtime.Gte.WriteControl(26, 256);
RecompOne.Runtime.Gte.Write(0, 4096u << 16);
RecompOne.Runtime.Gte.Write(1, 512);
RecompOne.Runtime.Gte.Execute(0x4A180001);
RecompOne.Runtime.Gte.ReadTo(cpu, 12, 14);
var projected = GteScreen.ValidatePacketVertex(cpu[12], cpu.GetGteVertexTag(12));
bool clippingPass = (short)(cpu[12] >> 16) == 1023 && projected.HasSubpixel && projected.ScreenY == 2168;
Console.WriteLine($"offscreen projection: nativeY={(short)(cpu[12] >> 16)} renderY={projected.ScreenY} " +
    $"expected=2168 {(clippingPass ? "PASS" : "FAIL")}");
if (!clippingPass) failures++;
GteScreen.VertexTag Project(short x, short y, short z)
{
    RecompOne.Runtime.Gte.Write(0, (ushort)x | (uint)(ushort)y << 16);
    RecompOne.Runtime.Gte.Write(1, (ushort)z);
    RecompOne.Runtime.Gte.Execute(0x4A180001);
    RecompOne.Runtime.Gte.ReadTo(cpu, 12, 14);
    return GteScreen.ValidatePacketVertex(cpu[12], cpu.GetGteVertexTag(12));
}
var nearTag = Project(100, 100, 64);
bool nearPass = (short)cpu[12] == 359 && (short)(cpu[12] >> 16) == 319 &&
    nearTag.HasSubpixel && nearTag.Depth == 64 && nearTag.ScreenX == 560 && nearTag.ScreenY == 520 &&
    !GteScreen.ValidatePacketVertex(cpu[12]+1, nearTag).HasSubpixel;
Console.WriteLine($"Near reciprocal saturation: native=({(short)cpu[12]},{(short)(cpu[12]>>16)}) host={nearTag} {(nearPass?"PASS":"FAIL")}");
if(!nearPass)failures++;
// A rotated straight edge must remain straight when an adjacent face uses its
// authored midpoint instead of the same subdivision. Compare in homogeneous
// coordinates; rounded IR/SZ and the native reciprocal break this identity.
RecompOne.Runtime.Gte.WriteControl(0, 3547);
RecompOne.Runtime.Gte.WriteControl(1, 2048);
RecompOne.Runtime.Gte.WriteControl(2, 4096);
RecompOne.Runtime.Gte.WriteControl(3, unchecked((ushort)(short)-2048));
RecompOne.Runtime.Gte.WriteControl(4, 3547);
var lineA = Project(-130, 107, 802);
uint nativeLineA = cpu[12];
var lineB = Project(274, 107, 1206);
uint nativeLineB = cpu[12];
var lineMid = Project(72, 107, 1004);
uint nativeLineMid = cpu[12];
double midX = (lineA.ScreenX * (double)lineA.Depth + lineB.ScreenX * (double)lineB.Depth) / (lineA.Depth + lineB.Depth);
double midY = (lineA.ScreenY * (double)lineA.Depth + lineB.ScreenY * (double)lineB.Depth) / (lineA.Depth + lineB.Depth);
bool linePass = nativeLineA == 0x009C0101 && nativeLineB == 0x0096018D && nativeLineMid == 0x0098014D && Math.Abs(lineMid.ScreenX - midX) < 0.00002 && Math.Abs(lineMid.ScreenY - midY) < 0.00002 &&
    Math.Abs(lineMid.Depth - (lineA.Depth + lineB.Depth) / 2) < 0.0001;
Console.WriteLine($"Rotated authored/subdivided edge: deviation=({lineMid.ScreenX-midX:R},{lineMid.ScreenY-midY:R}) native={nativeLineA:X8},{nativeLineB:X8},{nativeLineMid:X8} {(linePass ? "PASS" : "FAIL")}");
if (!linePass) failures++;
RecompOne.Runtime.Gte.WriteControl(0, 4096);
RecompOne.Runtime.Gte.WriteControl(1, 0);
RecompOne.Runtime.Gte.WriteControl(3, 0);
RecompOne.Runtime.Gte.WriteControl(4, 4096);
var planeA = Project(0, 0, 512);
var planeB = Project(4096, 0, 512);
var planeC = Project(0, 4096, 1024);
(double U, double V) SamplePlane(bool oldClamp)
{
    double right = oldClamp ? Math.Min(1023, planeB.ScreenX) : planeB.ScreenX;
    double bottom = oldClamp ? Math.Min(1023, planeC.ScreenY) : planeC.ScreenY;
    double b = (200 - planeA.ScreenX) / (right - planeA.ScreenX);
    double c = (200 - planeA.ScreenY) / (bottom - planeA.ScreenY);
    double a = 1 - b - c;
    double denominator = a / planeA.Depth + b / planeB.Depth + c / planeC.Depth;
    return (256 * b / planeB.Depth / denominator, 256 * c / planeC.Depth / denominator);
}
// Independent ray/plane intersection: z=512+y/8; at pixel(200,200),
// x/z=40/256 and y/z=80/256. The authored material maps one texel per16 units.
double hitZ = 512 / (1 - (80.0 / 256) / 8);
double expectedU = hitZ * (40.0 / 256) / 16;
double expectedV = hitZ * (80.0 / 256) / 16;
var oldSample = SamplePlane(true);
var newSample = SamplePlane(false);
bool planePass = Math.Abs(newSample.U - expectedU) < 1e-6 &&
    Math.Abs(newSample.V - expectedV) < 1e-6 && Math.Abs(oldSample.U - expectedU) > 1;
Console.WriteLine($"visible texture sample: old={oldSample} new={newSample} " +
    $"ray-plane=({expectedU},{expectedV}) {(planePass ? "PASS" : "FAIL")}");
if (!planePass) failures++;
RecompOne.Runtime.Gte.WriteControl(24, (274u << 16) - 1);
var boundaryTag = Project(0, 0, 512);
bool boundaryPass = (short)cpu[12] == 273 && boundaryTag.HasSubpixel && Math.Abs(boundaryTag.ScreenX - (274 - 1.0 / 65536)) < 0.00002 &&
    !GteScreen.ValidatePacketVertex(cpu[12] + 1, boundaryTag).HasSubpixel;
Console.WriteLine($"projection boundary: nativeX={(short)cpu[12]} renderX={boundaryTag.ScreenX:R} " +
    $"subpixel={boundaryTag.HasSubpixel} {(boundaryPass ? "PASS" : "FAIL")}");
if (!boundaryPass) failures++;
// Run the real subdivision routine with a vertex below its Y=510 software cap.
// The host must clip the true projection, not warp its visible triangle to that cap.
RecompOne.Runtime.Gte.WriteControl(0, 0);
RecompOne.Runtime.Gte.WriteControl(1, 799u << 16);
RecompOne.Runtime.Gte.WriteControl(2, 0);
RecompOne.Runtime.Gte.WriteControl(3, 512);
RecompOne.Runtime.Gte.WriteControl(4, 0);
CpuContext SubdivisionCpu() => new() { A1 = 0x1F800000, A2 = 0, A3 = 4096,
    T4 = 16, T5 = 128, T6 = 0, S5 = 0x3FFF3FFF, S6 = 0, SP = 0x80013000 };
var nativeSubdivision = SubdivisionCpu();
NativeGame.SubdivisionNative(nativeSubdivision, memory);
uint clampedWord = memory.ReadU32(0x1F800000);
memory.TryGetGteVertex(0x1F800000, clampedWord, out var uncertified);
bool lostAtCap = !GteScreen.ValidatePacketVertex(clampedWord, uncertified).HasSubpixel;
var fixedSubdivision = SubdivisionCpu();
NativeGame.Subdivision(fixedSubdivision, memory);
uint certifiedWord = memory.ReadU32(0x1F800000);
GteScreen.LoadU32(cpu, 25, memory, 0x1F800000);
RecompOne.Runtime.Gte.WriteFrom(cpu, 12, 25);
RecompOne.Runtime.Gte.StoreWord(memory, wholePacket, 12);
uint packetWord = memory.ReadU32(wholePacket);
memory.TryGetGteVertex(wholePacket, packetWord, out var certified);
certified = GteScreen.ValidatePacketVertex(packetWord, certified);
bool capPass = lostAtCap && clampedWord == certifiedWord &&
    nativeSubdivision.Snapshot().gpr.SequenceEqual(fixedSubdivision.Snapshot().gpr) &&
    certified.HasSubpixel && certified.ScreenY == 519.5f &&
    !GteScreen.ValidatePacketVertex(packetWord + 1, certified).HasSubpixel &&
    GteScreen.RamVertexTransform == null;
Console.WriteLine($"Engine subdivision software cap: native={certifiedWord:X8} projectionY={certified.ScreenY} retained={certified.HasSubpixel} {(capPass ? "PASS" : "FAIL")}");
if (!capPass) failures++;
// A subdivided edge must lie on its original projective segment, including when
// camera-space rounding would otherwise move it off an unsubdivided neighbour.
uint[] cornerAddresses = { 0x80018000u, 0x80018008u, 0x80018010u };
var cornerTags = new[] {
    new GteScreen.VertexTag(1000.25f, 20.25f, 40.75f, true),
    new GteScreen.VertexTag(1003.5f, 80.5f, 43.25f, true),
    new GteScreen.VertexTag(900, 30, 80, true) };
for (int i = 0; i < 3; i++)
{
    var t = cornerTags[i];
    uint w = (ushort)(short)MathF.Floor(t.ScreenX) | (uint)(ushort)(short)MathF.Floor(t.ScreenY) << 16;
    GteScreen.StoreU32(memory, cornerAddresses[i], w, t);
    memory.WriteU32(cornerAddresses[i]+4, (uint)t.Depth);
    memory.WriteU32(cornerAddresses[i]+0x1F40, (uint)(10+i) | (uint)(20+i*4)<<16);
}
CpuContext CornerCpu() => new() { T1=cornerAddresses[0], T2=cornerAddresses[1], T3=cornerAddresses[2] };
CpuContext EdgeSubdivisionCpu() => new() { A1=0x1F800000, A2=2, A3=2048, T4=16, T5=128,
    T6=0, S5=0x3FFF3FFF, S6=0, SP=0x80013000 };
NativeGame.CornersNative(CornerCpu(),memory);
var oldEdgeCpu=EdgeSubdivisionCpu();
NativeGame.SubdivisionNative(oldEdgeCpu,memory);
uint oldMidpoint=memory.ReadU32(0x1F800010);
NativeGame.Corners(CornerCpu(),memory);
var newEdgeCpu=EdgeSubdivisionCpu();
NativeGame.Subdivision(newEdgeCpu,memory);
uint newMidpoint=memory.ReadU32(0x1F800010);
memory.TryGetGteVertex(0x1F800010,newMidpoint,out var midpoint);
double expectedX=(cornerTags[0].ScreenX*(double)cornerTags[0].Depth+cornerTags[1].ScreenX*(double)cornerTags[1].Depth)/(cornerTags[0].Depth+cornerTags[1].Depth);
bool midpointPass=oldMidpoint==newMidpoint && oldEdgeCpu.Snapshot().gpr.SequenceEqual(newEdgeCpu.Snapshot().gpr) &&
    midpoint.HasSubpixel && Math.Abs(midpoint.ScreenX-expectedX)<0.00001 && midpoint.Depth==1001.875f;
GteScreen.LoadU32(cpu,25,memory,0x1F800010);
RecompOne.Runtime.Gte.WriteFrom(cpu,12,25);
RecompOne.Runtime.Gte.StoreWord(memory,wholePacket,12);
memory.TryGetGteVertex(wholePacket,memory.ReadU32(wholePacket),out var roundTripMidpoint);
midpointPass &= roundTripMidpoint==midpoint;
Console.WriteLine($"Engine projective subdivision edge: expectedX={expectedX} actual={midpoint} {(midpointPass ? "PASS" : "FAIL")}");
if(!midpointPass) failures++;
// A third corner behind the camera has no usable projection. It must not
// discard precision along the opposite visible edge (third weight is zero).
memory.WriteU32(cornerAddresses[2], memory.ReadU32(cornerAddresses[2]));
NativeGame.Corners(CornerCpu(), memory);
var partialEdgeCpu = EdgeSubdivisionCpu();
NativeGame.Subdivision(partialEdgeCpu, memory);
uint partialWord = memory.ReadU32(0x1F800010);
memory.TryGetGteVertex(0x1F800010, partialWord, out var partialMidpoint);
bool partialPass = partialWord == newMidpoint && partialMidpoint == midpoint &&
    partialEdgeCpu.Snapshot().gpr.SequenceEqual(newEdgeCpu.Snapshot().gpr);
Console.WriteLine($"Engine visible edge with unavailable third corner: {partialMidpoint} {(partialPass ? "PASS" : "FAIL")}");
if (!partialPass) failures++;
// A missing contributing endpoint must still fall back, never reuse the
// preceding triangle's corner. Compare the actual unmodified native routine.
memory.WriteU32(cornerAddresses[0], memory.ReadU32(cornerAddresses[0]));
NativeGame.CornersNative(CornerCpu(), memory);
NativeGame.SubdivisionNative(EdgeSubdivisionCpu(), memory);
uint fallbackWord = memory.ReadU32(0x1F800010);
memory.TryGetGteVertex(0x1F800010, fallbackWord, out var expectedFallback);
NativeGame.Corners(CornerCpu(), memory);
NativeGame.Subdivision(EdgeSubdivisionCpu(), memory);
memory.TryGetGteVertex(0x1F800010, memory.ReadU32(0x1F800010), out var actualFallback);
bool fallbackPass = memory.ReadU32(0x1F800010) == fallbackWord && actualFallback == expectedFallback;
Console.WriteLine($"Engine missing contributing corner keeps native fallback: {(fallbackPass ? "PASS" : "FAIL")}");
if (!fallbackPass) failures++;
// Exercise GP0 decoding, the real Engine world/HUD classifier, and HLE submission.
// Two neighboring textured triangles use different transfer paths for one edge.
var sink = new NumericBackend();
GpuHle.Active = true;
GpuHle.Backend = sink;
Recompiled.Wide.Install();
var gpu = new RecompOne.Runtime.Gpu();
foreach (bool preserve in new[] { false, true })
foreach (float distance in new[] { 1000f, 20000f })
{
    var points = new[] {
        new GteScreen.VertexTag(distance, 50.25f, 50.75f, true),
        new GteScreen.VertexTag(distance * 1.1f, 60.375f, 50.625f, true),
        new GteScreen.VertexTag(distance * 1.2f, 50.125f, 60.875f, true),
        new GteScreen.VertexTag(distance * 1.3f, 60.5f, 60.25f, true) };
    sink.Triangles.Clear();
    void Triangle(int a, int b, int c, bool registerTransfer)
    {
        gpu.WriteGp0(0x24808080);
        foreach (int index in new[] { a, b, c })
        {
            var t = points[index];
            uint word = (uint)(int)t.ScreenX | (uint)(int)t.ScreenY << 16;
            if (registerTransfer)
            {
                GteScreen.StoreU32(memory, source, word, t);
                GteScreen.LoadU32(cpu, 25, memory, source);
                if (preserve) RecompOne.Runtime.Gte.WriteFrom(cpu, 12, 25);
                else RecompOne.Runtime.Gte.Write(12, cpu[25]);
                RecompOne.Runtime.Gte.ReadTo(cpu, 8, 12);
                t = cpu.GetGteVertexTag(8);
                if (preserve)
                {
                    // The adjacent face also goes through SM1's native edge
                    // expansion. Its host edge must still coincide exactly.
                    GteScreen.StoreU32(memory, edgeAddress, word, t);
                    NativeGame.Edge(EdgeCpu(), memory);
                    word = memory.ReadU32(edgeAddress);
                    memory.TryGetGteVertex(edgeAddress, word, out t);
                }
            }
            gpu.WriteGp0(word, t);
            gpu.WriteGp0((uint)(index * 16));
        }
    }
    Triangle(0, 1, 2, false);
    Triangle(1, 2, 3, true);
    bool sharedPass = sink.Triangles.Count == 2;
    if (sharedPass)
    {
        var first = sink.Triangles[0]; var second = sink.Triangles[1];
        sharedPass = first.World && second.World &&
            first.B.X == second.A.X && first.B.Y == second.A.Y && first.B.Z == second.A.Z &&
            first.C.X == second.B.X && first.C.Y == second.B.Y && first.C.Z == second.B.Z &&
            second.A.X == points[1].ScreenX && second.B.Y == points[2].ScreenY &&
            first.A.HasGteZ && second.A.HasGteZ;
    }
    bool expected = sharedPass == preserve;
    Console.WriteLine($"GP0 shared edge depth={distance} repaired={preserve}: continuous={sharedPass} {(expected ? "PASS" : "FAIL")}");
    if (!expected) failures++;
}
// Chase Venom's web particles straddle -1024 after adding screen-space corners.
// Decode the real GP0 quad in both draw buffers, on either coordinate axis, while
// keeping ordinary particles and legitimate large saturated world surfaces.
foreach (int drawY in new[] { 0, 256 })
foreach (bool horizontal in new[] { false, true })
foreach (var (lo, hi, expected) in new[] {
    (-1028, -1021, 0), (1020, 1027, 0), (100, 107, 2), (-1024, 1023, 2) })
{
    gpu.WriteGp0(0xE5000000u | (uint)drawY << 11);
    sink.Triangles.Clear();
    gpu.WriteGp0(0x2E808080);
    for (int i = 0; i < 4; i++)
    {
        int x = horizontal ? ((i & 1) == 0 ? lo : hi) : 259 + (i & 1) * 7;
        int y = horizontal ? 100 + (i >> 1) * 7 : ((i >> 1) == 0 ? lo : hi);
        uint word = (ushort)(short)x | (uint)(ushort)(short)y << 16;
        gpu.WriteGp0(word, GteScreen.VertexTag.DepthOnly(560.74365f));
        gpu.WriteGp0((uint)(96 + (i & 1) * 24) | (uint)(208 + (i >> 1) * 24) << 8 |
                     (i < 2 ? 40u << 16 : 0));
    }
    bool pass = sink.Triangles.Count == expected;
    Console.WriteLine($"GP0 effect wrap drawY={drawY} horizontal={horizontal} bounds={lo}..{hi}: triangles={sink.Triangles.Count} {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
gpu.WriteGp0(0xE5000000);
foreach (bool completeProjection in new[] { false, true })
{
    sink.Triangles.Clear();
    gpu.WriteGp0(0x2E808080);
    for (int i = 0; i < 4; i++)
    {
        int x = 259 + (i & 1) * 7, y = (i >> 1) == 0 ? -1028 : -1021;
        uint word = (ushort)(short)x | (uint)(ushort)(short)y << 16;
        var tag = new GteScreen.VertexTag(560, x + 0.25f, y + 0.75f, true)
            { NativeScreen = word | 0x80000000u };
        if (!completeProjection && i == 0) tag = GteScreen.VertexTag.DepthOnly(560);
        gpu.WriteGp0(word, tag);
        gpu.WriteGp0((uint)(96 + (i & 1) * 24) | (uint)(208 + (i >> 1) * 24) << 8 |
                     (i < 2 ? 40u << 16 : 0));
    }
    bool pass = sink.Triangles.Count == (completeProjection ? 2 : 1) &&
        sink.Triangles.All(t => t.A.Y < -1020 && t.B.Y < -1020 && t.C.Y < -1020);
    Console.WriteLine($"GP0 wrapped packet with complete projection={completeProjection}: precise XY retained {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
// Exercise the actual native sphere + AABB object selector in the newly visible
// horizontal margin. Outside-wide, vertical and behind-camera bounds still cull.
foreach(var (x,y,z,wantWide) in new[] {(1100,0,1000,true),(1600,0,1000,false),(0,1800,1000,false),(0,0,-1000,false),(0,0,1000,true)})
{
    const uint obj=0x80011000, mesh=0x80012000, table=0x80013000, camera=0x80014000;
    memory.WriteU32(obj+4,unchecked((uint)(x<<12)));
    memory.WriteU32(obj+8,unchecked((uint)(y<<12)));
    memory.WriteU32(obj+12,unchecked((uint)(z<<12)));
    memory.WriteU32(obj+16,0);memory.WriteU32(obj+20,0);
    memory.WriteU32(obj+24,0);memory.WriteU32(obj+28,0);
    memory.WriteU32(NativeGame.ModelTable,table); memory.WriteU32(table,mesh);
    memory.WriteU32(mesh+8,20u<<12);
    for(uint i=12;i<=20;i+=4)memory.WriteU32(mesh+i,unchecked((ushort)-10)|(10u<<16));
    memory.WriteU32(NativeGame.Camera,camera);
    for(uint i=4;i<=12;i+=4)memory.WriteU32(camera+i,0);
    void Matrix(int first,short[] values)
    {
        for(int i=0;i<5;i++)RecompOne.Runtime.Gte.WriteControl(first+i,(ushort)values[2*i]|((uint)(ushort)values[2*i+1]<<16));
    }
    Matrix(8,[0,0,-4096,0,0,4096,0,2896,2896,0]);
    Matrix(16,[0,-2896,2896,2896,0,2896,-2896,0,2896,0]);
    RecompOne.Runtime.Gte.WriteControl(13,5000);RecompOne.Runtime.Gte.WriteControl(14,unchecked((uint)-1));RecompOne.Runtime.Gte.WriteControl(15,0);
    var saved=Enumerable.Range(16,5).Select(RecompOne.Runtime.Gte.ReadControl).ToArray();
    bool Visible(bool wide)
    {
        memory.WriteU16(obj,0);
        GpuHle.FovNum=wide?1000:1; GpuHle.FovDen=wide?1333:1;
        NativeGame.Frustum(new CpuContext {A0=obj,SP=0x801F0000},memory);
        return (memory.ReadU16(obj)&0x8000)==0;
    }
    bool native=Visible(false), wide=Visible(true);
    bool pass=wide==wantWide && (x!=1100 || !native) && saved.SequenceEqual(Enumerable.Range(16,5).Select(RecompOne.Runtime.Gte.ReadControl));
    Console.WriteLine($"Native object frustum ({x},{y},{z}): 4:3={native} 16:9={wide}, restored matrices {pass} {(pass?"PASS":"FAIL")}");
    if(!pass)failures++;
}
GpuHle.FovNum=GpuHle.FovDen=1;
foreach(bool reverse in new[]{false,true})
{
    var thin=new[]{new GteScreen.VertexTag(900,10.1f,20.1f,true),new GteScreen.VertexTag(900,15.1f,20.1f,true),new GteScreen.VertexTag(900,12.1f,20.8f,true)};
    if(reverse)Array.Reverse(thin);
    for(int i=0;i<3;i++)
    {
        uint word=(uint)(int)thin[i].ScreenX|((uint)(int)thin[i].ScreenY<<16);
        GteScreen.StoreU32(memory,source+(uint)i*4,word,thin[i]);
        RecompOne.Runtime.Gte.Write(12+i,word);
    }
    RecompOne.Runtime.Gte.Execute(0x4B400006);
    int native=(int)RecompOne.Runtime.Gte.Read(24);
    for(int i=0;i<3;i++)RecompOne.Runtime.Gte.LoadWord(memory,source+(uint)i*4,12+i);
    RecompOne.Runtime.Gte.Execute(0x4B400006);
    int precise=(int)RecompOne.Runtime.Gte.Read(24);
    bool pass=native==0 && precise==(reverse?-1:1);
    Console.WriteLine($"Thin joint face winding reverse={reverse}: native={native} precise={precise} {(pass?"PASS":"FAIL")}");
    if(!pass)failures++;
}
var depthOnly=GteScreen.VertexTag.DepthOnly(700);
GteScreen.StoreU32(memory,source,25u|30u<<16,depthOnly);
RecompOne.Runtime.Gte.LoadWord(memory,source,14);
RecompOne.Runtime.Gte.ReadTo(cpu,12,14);
RecompOne.Runtime.Gte.StoreWord(memory,wholePacket,14);
memory.TryGetGteVertex(wholePacket,25u|30u<<16,out var roundTripDepth);
bool depthOnlyPass=cpu.GetGteVertexTag(12).Depth==700 && !cpu.GetGteVertexTag(12).HasSubpixel && roundTripDepth.Depth==700 && !roundTripDepth.HasSubpixel;
Console.WriteLine($"Depth-only screen provenance stays non-fractional {(depthOnlyPass?"PASS":"FAIL")}");
if(!depthOnlyPass)failures++;

// A sprite never has GTE provenance, even immediately after a world polygon.
gpu.WriteGp0(0x24808080);
foreach (var (x, y) in new[] { (50, 50), (60, 50), (50, 60) })
{
    gpu.WriteGp0((uint)x | (uint)y << 16, new GteScreen.VertexTag(1000, x, y, true));
    gpu.WriteGp0(0);
}
gpu.WriteGp0(0x64808080);
gpu.WriteGp0(20u | 20u << 16);
gpu.WriteGp0(12u | 34u << 8);
gpu.WriteGp0(8u | 8u << 16);
bool spritePass = sink.Rectangles.Count > 0 && !sink.Rectangles[^1].Flags.World;
Console.WriteLine($"Rectangle after world polygon stays screen-space: {(spritePass ? "PASS" : "FAIL")}");
if (!spritePass) failures++;

#if !SM2
var oldWide = RecompOne.Runtime.Config.ConfigManager.Game.Widescreen;
RecompOne.Runtime.Config.ConfigManager.Game.Widescreen = true;
RecompOne.Runtime.Sdk.LibGpu.LastDispGrandparent = 0x8002C2AC;
RecompOne.Runtime.Events.Event.Dispatch(new RecompOne.Runtime.Events.VSyncEvent { Frame = 1 });
foreach (int drawY in new[] { 0, 256 })
foreach (int x in new[] { 138, 174, 192, 246, 264, 462 })
{
    var e = new RecompOne.Runtime.Events.RenderPrimEvent {
        Count = 4, Textured = true, TexPage = 8, Clut = 419,
        DrawRight = 511, DrawTop = drawY, DrawBottom = drawY + 239,
    };
    for (int i = 0; i < 4; i++)
    {
        e.X[i] = x + (i % 2) * 18; e.Y[i] = drawY + 28 + (i / 2) * 7;
        e.U[i] = 240 + (i % 2) * 11; e.V[i] = 248 + (i / 2) * 7;
    }
    RecompOne.Runtime.Events.Event.Dispatch(e);
    bool pass = e.Hud && !e.World && e.X[0] == 511 + (x - 511) * 1000 / 1333 &&
        e.X[1] == 511 + (x + 18 - 511) * 1000 / 1333;
    Console.WriteLine($"Chase rail common anchor x={x} drawY={drawY}: {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
foreach (var (page, clut, u, v, uw, vh) in new[] {
    (8, 418, 108, 248, 3, 7), (12, 418, 52, 168, 3, 7),
    (136, 3552, 126, 152, 17, 25), (138, 3616, 32, 208, 29, 29) })
foreach (bool world in new[] { false, true })
{
    var e = new RecompOne.Runtime.Events.RenderPrimEvent {
        Count = 4, Textured = true, TexPage = page, Clut = clut,
        DrawRight = 511, DrawBottom = 239,
    };
    for (int i = 0; i < 4; i++)
    {
        e.X[i] = 240 + (i % 2) * 30; e.Y[i] = 16 + (i / 2) * vh;
        e.U[i] = u + (i % 2) * uw; e.V[i] = v + (i / 2) * vh;
        e.HasDepth[i] = world; e.Depth[i] = world ? 1000 : 0;
    }
    RecompOne.Runtime.Events.Event.Dispatch(e);
    bool pass = e.World == world && e.Hud == !world &&
        e.X[0] == (world ? 240 : 511 + (240 - 511) * 1000 / 1333);
    Console.WriteLine($"Chase cap/marker page={page} clut={clut} world={world}: {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
RecompOne.Runtime.Config.ConfigManager.Game.Widescreen = oldWide;
GpuHle.FovNum = GpuHle.FovDen = 1;
#endif
#if SM2
const uint menuFrame = 0x800C247C, menuCursor = 0x800C2484, menuLimit = 0x800C2034,
    menuTable = 0x800A184C, menuPoly = 0x80075510, menuOther = 0x80075514,
    menuGp = 0x800C0000, menuRamp = menuGp + 0x1010;
#else
const uint menuFrame = 0x800B54A8, menuCursor = 0x800B54B0, menuLimit = 0x800B4FE8,
    menuTable = 0x80095194, menuPoly = 0x8006A2F4, menuOther = 0x8006A2F8,
    menuGp = 0x800B47F4, menuRamp = menuGp + 0xEC8;
#endif
foreach (var (polygon, singleMetadata) in new[] { (false, true), (false, false), (true, false) })
foreach (uint initialRamp in new uint[] { 0, 0x2E0, 0x300 })
{
    const uint frame = 0x80098000, ot = 0x80099000, head = 0x00300000,
        drawable = head + 0x40;
    uint tail = singleMetadata ? head : head + 0x80;
    memory.WriteU32(menuFrame, frame);
    memory.WriteU32(frame + 0x70, ot);
    memory.WriteU32(ot, head);
    memory.WriteU32(ot + 4, ot & 0xFFFFFF);
    memory.WriteU32(head, 0x02000000 | (polygon ? drawable : tail));
    memory.WriteU32(head + 4, 0xE3000000);
    memory.WriteU32(head + 8, 0xE403BDFF);
    memory.WriteU32(drawable, 0x09000000 | tail);
    memory.WriteU32(drawable + 4, 0x2C808080);
    memory.WriteU32(tail, 0x02FFFFFF);
    memory.WriteU32(tail + 4, 0xE3000000);
    memory.WriteU32(tail + 8, 0xE403BDFF);
    for (uint i = 0; i < 29; i++)
        memory.WriteU32(menuTable + i * 4, i % 4 == 0 ? menuPoly : menuOther);
    memory.WriteU32(menuCursor, head + 0x100);
    memory.WriteU32(menuLimit, 0); // Exercise relinking without generating backdrop art.
    memory.WriteU32(menuRamp, initialRamp);
    cpu = new CpuContext { GP = menuGp, SP = 0x807E0000, RA = 0x80010000 };
#if SM2
    Recompiled.SpiderMan2.func_80075438(cpu, memory);
#else
    Recompiled.SpiderMan.func_8006A21C(cpu, memory);
#endif
    var visited = new HashSet<uint>();
    var order = new List<uint>();
    uint node = (ot + 4) & 0xFFFFFF;
    while (node != 0xFFFFFF && visited.Count < 32 && visited.Add(node))
    {
        order.Add(node);
        node = memory.ReadU32(node) & 0xFFFFFF;
    }
    uint[] expectedOrder = singleMetadata ? [(ot + 4) & 0xFFFFFF, head, ot & 0xFFFFFF] :
        polygon ? [(ot + 4) & 0xFFFFFF, head, drawable, ot & 0xFFFFFF, tail] :
        [(ot + 4) & 0xFFFFFF, head, ot & 0xFFFFFF, tail];
    bool pass = node == 0xFFFFFF && visited.Contains(head) && visited.Contains(tail) &&
        order.SequenceEqual(expectedOrder) && memory.ReadU32(head + 4) == 0xE3000000 &&
        memory.ReadU32(tail + 8) == 0xE403BDFF &&
        memory.ReadU32(menuRamp) == Math.Min(initialRamp + 0x60, 0x300u);
    Console.WriteLine($"Native Pause OT polygon={polygon} singleMetadata={singleMetadata} ramp={initialRamp:X}: terminates={node == 0xFFFFFF} nodes={visited.Count} {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
#if SM2
// Native Pause deliberately desaturates untextured scene primitives.
foreach (bool paused in new[] { false, true })
{
    const uint frame = 0x80098000, ot = 0x80099000, primitive = 0x00300000;
    memory.WriteU32(menuFrame, frame);
    memory.WriteU32(frame + 0x70, ot);
    memory.WriteU32(ot + 0x3FFC, primitive);
    memory.WriteU32(ot, primitive + 0x100);
    memory.WriteU32(ot + 4, ot & 0xFFFFFF);
    memory.WriteU32(primitive + 0x100, 0x02FFFFFF);
    memory.WriteU32(primitive + 0x104, 0xE3000000);
    memory.WriteU32(primitive + 0x108, 0xE403BDFF);
    memory.WriteU32(primitive, 0x04FFFFFF);
    memory.WriteU32(primitive + 4, 0x20FF0000);
    memory.WriteU32(0x800A18C4, 0x80075978);
    memory.WriteU32(menuGp + 0x1004, paused ? 1u : 0u);
    cpu = new CpuContext { GP = menuGp, SP = 0x807E0000, RA = 0x80010000 };
    Recompiled.SpiderMan2.func_8007590C(cpu, memory);
    uint color = memory.ReadU32(primitive + 4);
    bool pass = color == (paused ? 0x20545454u : 0x20FF0000u);
    Console.WriteLine($"Native Pause grayscale paused={paused}: color={color:X8} {(pass ? "PASS" : "FAIL")}");
    if (!pass) failures++;
}
#endif
return failures == 0 ? 0 : 1;

sealed class NumericBackend : IGpuBackend
{
    public readonly List<(HleVertex A, HleVertex B, HleVertex C, bool World)> Triangles = [];
    public readonly List<(HleRect Rect, PrimFlags Flags)> Rectangles = [];
    public bool Ready => true;
    public void SetDrawEnv(in HleDrawEnv env) { }
    public void DrawTri(in HleVertex a, in HleVertex b, in HleVertex c, in PrimFlags f) => Triangles.Add((a,b,c,f.World));
    public void DrawRect(in HleRect r, in PrimFlags f) => Rectangles.Add((r, f));
    public void DrawLine(in HleVertex a, in HleVertex b, in PrimFlags f) { }
    public void FillRect(int x, int y, int w, int h, ushort color15) { }
    public void CopyVram(int sx, int sy, int dx, int dy, int w, int h) { }
    public void WriteVram(int x, int y, int w, int h, ReadOnlySpan<ushort> px) { }
    public void ReadVram(int x, int y, int w, int h, Span<ushort> px) { }
    public int RegisterImage(ReadOnlySpan<byte> rgba, int width, int height) => 0;
    public void Flush() { }
    public void Present(in HleDispEnv disp) { }
}
