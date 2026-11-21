using System;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>USA ROM 8003632C / 800364EC / 800378B0 visibility decisions.</summary>
public static class N64ShadowVisibility
{
    // Decoded from the ROM's story table (800E4E50) and training table
    // (800E4D84). The training loader at 800329B4 subtracts 50, not 34.
    static readonly string[] Story = "l1a1,l1a2,l1a3,l1a4,l2a1,l2a2,l3a1,l3a2,l3a3,l3a4,l3a5,l4a1,l5a1,l5a2,l5a3,l5a4,l5a5,l5a6,l5a7,l6a1,l6a2,l6a3,l6a4,l7a1,l7a2,l7a3,l7a4,l7a5,l8a1,l8a2,l8a3,l8a4,l8a5,l8a6".Split(',');
    static readonly string[] Training = "l9a1,l9a2,l9a3,l9a4,lba1,lba2,lba3,lba4,lca1,lca2,lca3,lca4,lda1,lda2,lda3,lga1,lha1".Split(',');

    public static (int Level, bool Demo) Level(IMemory m)
    {
        Span<char> name = stackalloc char[4];
        for (uint i=0;i<4;i++) name[(int)i] = char.ToLowerInvariant((char)m.ReadU8(0x800A568C+i));
        if (name[..3].SequenceEqual("dem")) return (-1,true);
        for (int i=0;i<Story.Length;i++) if (name.SequenceEqual(Story[i])) return (i,false);
        for (int i=0;i<Training.Length;i++) if (name.SequenceEqual(Training[i])) return (50+i,false);
        return (-1,false);
    }

    public static ShadowScene Scene(int level, bool demo) => demo ? ShadowScene.Ordinary : level switch
    {
        0x13 or 0x15 => ShadowScene.HideWhileAttached,
        0x18 => ShadowScene.PixelDepth,
        0x42 => ShadowScene.ZipLineTraining,
        _ => ShadowScene.Ordinary
    };

    public static bool Script(bool hidden, int level, uint script, int opcode, uint cinematic)
    {
        if (opcode==0xC7) return level==25 && script==0x15D ? false : hidden;
        if (opcode!=0xC3) return hidden;
        if (cinematic==0) return false;
        return hidden || (level==2 && script==0xEA);
    }

}
