using RecompOne.Runtime.Cdrom;

// Every sector belongs to a different XA channel. A filtered reader must still
// advance at disc speed rather than scanning the whole image in milliseconds.
sealed class FilteredDisc : IDiscImage
{
    public int Reads;
    public volatile bool BlockRead;
    public readonly ManualResetEventSlim ReadEntered = new();
    public readonly ManualResetEventSlim ReadRelease = new();
    public string Format => "synthetic XA";
    public int FirstTrack => 1;
    public int LastTrack => 1;
    public bool HasTracks => true;
    public int LeadoutLba => 100000;
    public int DataSectors => LeadoutLba;
    public IReadOnlyList<DiscTrack> Tracks => [new(1, DiscTrackKind.Data, 0, 2352)];
    public bool TrackStartLba(int track, out int lba) { lba = 0; return track == 1; }
    public byte[] ReadSectorData(int lba, int size)
    {
        Interlocked.Increment(ref Reads);
        if (BlockRead)
        {
            ReadEntered.Set();
            if (!ReadRelease.Wait(5000)) throw new TimeoutException("test XA read gate");
        }
        var sector = new byte[size];
        sector[0] = 1; sector[1] = 15; sector[2] = 4;
        return sector;
    }
    public void Dispose() { }
}
