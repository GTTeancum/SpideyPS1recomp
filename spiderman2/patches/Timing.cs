using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Sdk;

namespace Recompiled;

public static class Timing
{
    // Retail 80069124 is the equivalent interrupt-counter wait in SM2.
    public static void WaitVBlanks(CpuContext c, IMemory m)
        => LibEtc.WaitCounter(c, m, c.GP + 0xCD0u);
}
