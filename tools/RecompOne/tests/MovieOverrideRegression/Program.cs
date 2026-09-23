using System.Security.Cryptography;
using RecompOne.Runtime.Media;
using RecompOne.Runtime.Host;
using Fixture;

int passed=0;
void Check(bool ok,string name) { if(!ok) throw new Exception("FAIL "+name); passed++; Console.WriteLine("PASS "+name); }
string root=Path.Combine(Path.GetTempPath(),"OpenSpidey-SFD-"+Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);State.Root=root;
string dir=Path.Combine(root,"mods","movies","dreamcast");Directory.CreateDirectory(dir);
string path=Path.Combine(dir,"L1M1.SFD");byte[] good=File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"tiny.sfd"));File.WriteAllBytes(path,good);
string originalHash=Convert.ToHexString(SHA256.HashData(good));
string? prior=Environment.GetEnvironmentVariable("SPIDEY_MOVIE_DIR"),priorEnabled=Environment.GetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES");
Environment.SetEnvironmentVariable("SPIDEY_MOVIE_DIR",null);Environment.SetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES",null);
try
{
    using(var sfd=new NativeSfd(path))
    {
        Check(sfd.Width==32 && sfd.Height==32 && sfd.AudioFrames==88200 && sfd.AudioDuration==2,"original SFD metadata");
        byte[] rgba=new byte[32*32*4];short[] pcm=new short[4096];
        Check(sfd.ReadFrameRgba(rgba) && sfd.DecodedFrames==1 && rgba[3]==255,"first MPEG frame decodes directly");
        Check(sfd.ReadAudio(pcm,2048)==2048 && pcm.All(x=>x==0),"original silent ADX test pattern");
        Check(sfd.ReadFrameRgba(rgba) && sfd.ReadAudio(pcm,2048)==2048,"interleaved audio/video cursors");
        while(sfd.ReadFrameRgba(rgba)) { }
        while(sfd.ReadAudio(pcm,2048)!=0) { }
        Check(sfd.DecodedFrames==60 && sfd.VideoEnded && sfd.AudioFramesRead==88200,"last reference frame and exact ADX sample count");
        Check(!sfd.ReadFrameRgba(rgba) && sfd.ReadAudio(pcm,2048)==0,"stable EOF");
    }
    Check(MovieResolver.Resolve(@"\MOVIES\l1m1.str;1",dir)==path,"case-insensitive SFD/Windows ISO mapping");
    Check(MovieResolver.Resolve("LOGO.STR",dir)==null && MovieResolver.Resolve("UNKNOWN.STR",dir)==null,"no speculative movie aliases");
    Check(MovieResolver.Resolve("L1M1.SFD",dir)==null,"hook accepts native STR names only");
    File.WriteAllBytes(Path.Combine(dir,"L1M2.osmv"),good);
    Check(MovieResolver.Resolve("L1M2.STR",dir)==null,"old cache is not a playback input");
    if(!OperatingSystem.IsWindows())
    {
        string duplicate=Path.Combine(dir,"l1m1.sfd");File.WriteAllBytes(duplicate,good);
        Check(MovieResolver.Resolve("L1M1.STR",dir)==null,"ambiguous case rejected");File.Delete(duplicate);
    }
    Check(MovieTiming.FrameAt(1,30000,1001,100)==29 && MovieTiming.FrameAt(1,30,1,100)==30,"rational frame timing");
    var timing=new MovieTiming(true);
    Check(!timing.Skip(2,true) && !timing.Skip(2.1,false) && timing.Skip(2.2,true),"entry button must be released");
    timing=new(false);Check(!timing.Skip(.5,true) && !timing.Skip(1.1,true) && !timing.Skip(1.2,false) && timing.Skip(1.3,true),"early press not deferred");
    foreach(string mode in new[]{"","skip","audio-error","begin-error","reset"})
    {
        State.Reset(mode);MovieOverrideResult result=MovieOverrideResult.Unavailable;
        bool reset=false;try { result=DreamcastMovies.TryPlay(@"\MOVIES\L1M1.STR;1"); }catch(ResetSignal) { reset=true; }
        Check(reset==(mode=="reset"),"reset propagation: "+mode);
        Check(!DreamcastMovies.Active && State.Disposed==State.Opened,"audio ownership released: "+mode);
        Check(State.StreamHeld==State.StreamDisposed && State.StreamHeld==(mode=="begin-error"?0:1),"native producer hold cleanup: "+mode);
        Check(State.Cleared==(mode=="begin-error"?0:1) && State.Resynced==State.Cleared,"display/clock cleanup: "+mode);
        if(mode=="") Check(result==MovieOverrideResult.Played && State.Seconds>=2,"stream reaches final frame/sample");
        if(mode=="skip") Check(result==MovieOverrideResult.Played && State.Seconds<2,"skip uses native cleanup branch");
        if(mode=="audio-error") Check(result==MovieOverrideResult.Failed,"late error requests STR restart");
        if(mode=="begin-error") Check(result==MovieOverrideResult.Unavailable,"pre-start failure leaves STR untouched");
    }
    State.Reset();HostWindow.IsHeadless=true;Check(DreamcastMovies.TryPlay("L1M1.STR")==MovieOverrideResult.Unavailable && State.Opened==0,"headless native fallback");HostWindow.IsHeadless=false;
    Environment.SetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES","0");Check(DreamcastMovies.TryPlay("L1M1.STR")==MovieOverrideResult.Unavailable,"disabled native fallback");Environment.SetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES",null);
    Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)))==originalHash,"input SFD remains byte-identical");
    byte[] broken=(byte[])good.Clone();broken[0]^=1;File.WriteAllBytes(path,broken);State.Reset();
    Check(DreamcastMovies.TryPlay("L1M1.STR")==MovieOverrideResult.Unavailable && State.Uploaded==0,"bad SFD rejected before playback");
    broken=(byte[])good.Clone();int terminator=-1;
    for(int i=broken.Length-4;i>=0;i--) if(broken[i]==128 && broken[i+1]==1 && broken[i+2]==0 && broken[i+3]==0) { terminator=i; break; }
    Check(terminator>0,"authored ADX terminator located");broken[terminator+1]=2;File.WriteAllBytes(path,broken);State.Reset();
    Check(DreamcastMovies.TryPlay("L1M1.STR")==MovieOverrideResult.Failed && State.Disposed==1 && State.Cleared==1,"late ADX failure releases resources and requests STR restart");
    foreach(string folder in args) foreach(string f in Directory.EnumerateFiles(folder).Where(p=>p.EndsWith(".sfd",StringComparison.OrdinalIgnoreCase)))
    {
        using var sfd=new NativeSfd(f);byte[] rgba=new byte[sfd.Width*sfd.Height*4];short[] pcm=new short[8192];
        while(sfd.ReadFrameRgba(rgba)) { }
        while(sfd.ReadAudio(pcm,4096)!=0) { }
        Check(sfd.AudioFramesRead==sfd.AudioFrames && sfd.DecodedFrames>0,"all original streams: "+Path.GetFileName(f));
    }
    Console.WriteLine($"{passed} managed/native checks passed. Host devices are fixtures, NOT gameplay.");return 0;
}
finally
{
    Environment.SetEnvironmentVariable("SPIDEY_MOVIE_DIR",prior);Environment.SetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES",priorEnabled);
    Directory.Delete(root,true);
}
