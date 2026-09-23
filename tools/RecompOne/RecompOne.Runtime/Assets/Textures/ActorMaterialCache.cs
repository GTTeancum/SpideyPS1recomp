using RecompOne.Runtime.Memory;

namespace RecompOne.Runtime.Assets.Textures;

/// <summary>Loader-owned material snapshot, valid only for one ordering-table submission.</summary>
public sealed class ActorMaterialCache(uint table)
{
    readonly Dictionary<(int, int, int, int, int), List<ResolvedTexture>> _bindings = new();
    long _draw = -1;
    IMemory? _memory;
    IReadOnlyDictionary<uint, ReplacementTexture>? _textures;
    public void Invalidate() => _draw = -1;
    static bool Ram(uint p, uint size) => p >= 0x80000000 && size <= 0x800000 && p <= 0x80800000 - size;
    static (int, int, int, int, int) Key(TileRect tile) => (tile.PageX, tile.PageY, tile.Bpp, tile.ClutX, tile.ClutY);

    public ResolvedTexture Resolve(IMemory memory, IReadOnlyDictionary<uint, ReplacementTexture> textures, TileRect tile)
    {
        if (_draw != Sdk.LibGpu.OtCount || !ReferenceEquals(_memory, memory) || !ReferenceEquals(_textures, textures))
        {
            _draw = Sdk.LibGpu.OtCount; _memory = memory; _textures = textures;
            Build(memory, textures);
        }
        if (_bindings.TryGetValue(Key(tile), out var entries))
            foreach (var binding in entries)
            {
                var rect = binding.Rect;
                if (tile.U0 >= rect.U0 && tile.V0 >= rect.V0 && tile.U0 < rect.U0 + rect.W && tile.V0 < rect.V0 + rect.H &&
                    tile.U0 + tile.W <= rect.U0 + rect.W + 1 && tile.V0 + tile.H <= rect.V0 + rect.H + 1)
                    return binding;
            }
        return default;
    }

    void Build(IMemory m, IReadOnlyDictionary<uint, ReplacementTexture> textures)
    {
        _bindings.Clear();
        uint entry = 0;
        for (uint i = 0; i < 40; i++)
        {
            uint p = table + i * 64;
            if (m.ReadU32(p) == 0x64697073 && m.ReadU16(p + 4) == 0x7965 && m.ReadU8(p + 6) == 0) { entry = p; break; }
        }
        if (entry == 0) return;
        uint model = m.ReadU32(entry + 0x14);
        if (!Ram(model, 16)) return;
        uint objects = m.ReadU32(model + 8);
        if (objects > 1024 || !Ram(model, 16 + objects * 36)) return;
        uint meshes = m.ReadU32(model + 12 + objects * 36);
        uint metadata = m.ReadU32(entry + 12);
        if (meshes > 4096 || !Ram(metadata, meshes * 4 + 4)) return;
        uint start = metadata + meshes * 4;
        uint count = m.ReadU32(start);
        if (count > 4096 || !Ram(start, 4 + count * 4)) return;
        for (uint i = 0; i < count; i++)
        {
            uint p = m.ReadU32(start + 4 + i * 4);
            if (!Ram(p, 32) || !textures.TryGetValue(m.ReadU32(p + 20), out var texture)) continue;
            int u = m.ReadU8(p), v = m.ReadU8(p + 1);
            int w = ((m.ReadU8(p + 4) - u) & 255) + 1;
            int h = ((m.ReadU8(p + 9) - v) & 255) + 1;
            var rect = TextureTile.Describe(m.ReadU16(p + 6), m.ReadU16(p + 2), u, v, w, h);
            if (!_bindings.TryGetValue(Key(rect), out var list)) _bindings[Key(rect)] = list = new();
            list.Add(new ResolvedTexture { Texture = texture, Rect = rect, Hit = true });
        }
    }
}
