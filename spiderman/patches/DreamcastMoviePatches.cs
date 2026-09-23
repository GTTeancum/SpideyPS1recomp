using System.Text;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using RecompOne.Runtime.Media;

namespace Recompiled;

/// <summary>
/// SM1 USA only: replace the STR decode/present loop, not movie setup/cleanup.
/// 8002AD0C is after native allocation, display setup, STR ring setup and input
/// polling; true branches to 8002AF8C, the normal native stop/free/restore path.
/// </summary>
public static class DreamcastMoviePatches
{
    public static bool Play(CpuContext c, IMemory m)
    {
        uint movie=m.ReadU8(c.GP+0x6D1u);
        if(movie>=24 || m.ReadU32(c.GP+0x690u)==0) return false;
        uint name=m.ReadU32(0x80097DECu+movie*24);
        if(name<0x80000000u || name>0x801FFF80u) return false;
        var text=new StringBuilder(64);
        bool terminated=false;
        for(uint i=0;i<128;i++)
        {
            byte b=m.ReadU8(name+i);
            if(b==0) { terminated=true; break; }
            if(b<32 || b>126) return false;
            text.Append((char)b);
        }
        if(!terminated) return false;
        MovieOverrideResult result=DreamcastMovies.TryPlay(text.ToString());
        if(result==MovieOverrideResult.Failed)
        {
            // A late decoder/device failure must restart STR at its original LBA,
            // not continue with the CD cursor advanced during host playback.
            SpiderMan.func_8002B1FC(c,m);
            return false;
        }
        return result==MovieOverrideResult.Played;
    }
}
