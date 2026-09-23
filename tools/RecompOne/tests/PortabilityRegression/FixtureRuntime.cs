// Minimal dependencies for executing the REAL shipping loose loader in a BCL-only
// regression. No game, disc importer, emulator CPU, or renderer is being simulated.
namespace RecompOne.Runtime.Context
{
    public sealed class CpuContext { public uint A0, V0; }
}
namespace RecompOne.Runtime.Memory
{
    public interface IMemory { void ZeroRange(uint address, uint length); void LoadBytes(uint address, byte[] data); }
    public sealed class CaptureMemory : IMemory
    {
        public byte[] Bytes = Array.Empty<byte>();
        public int Writes;
        public void ZeroRange(uint address, uint length) { Bytes = new byte[checked((int)length)]; ++Writes; }
        public void LoadBytes(uint address, byte[] data) { data.CopyTo(Bytes, 0); ++Writes; }
    }
}
namespace RecompOne.Runtime.Cdrom
{
    public static class LooseDiscImporter
    {
        public static void EnsureWad(string root)
        {
            if (!Directory.Exists(Path.Combine(root, "wad"))) throw new DirectoryNotFoundException("fixture WAD directory missing");
        }
    }
}
