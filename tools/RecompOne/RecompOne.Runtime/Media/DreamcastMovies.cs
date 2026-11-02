using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Host;

namespace RecompOne.Runtime.Media;

public enum MovieOverrideResult { Unavailable, Played, Failed }

/// <summary>Original .SFD streaming playback inside SM1's real STR routine.</summary>
public static class DreamcastMovies
{
    public static bool Active { get; private set; }
    /// <summary>Where original .SFD movies are read from: SPIDEY_MOVIE_DIR, else mods/movies/dreamcast beside the game.</summary>
    public static string Folder=>Environment.GetEnvironmentVariable("SPIDEY_MOVIE_DIR") ??
        Path.Combine(RuntimePaths.ApplicationDirectory,"mods","movies","dreamcast");

    /// <summary>Startup note of where the player looks and how many movies it found.</summary>
    public static void ReportFolder()
    {
        if(Environment.GetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES")=="0")
        {
            Console.WriteLine("[movie] Dreamcast movies disabled (SPIDEY_DREAMCAST_MOVIES=0); PS1 movies only");
            return;
        }
        string dir=Folder;
        int found=Directory.Exists(dir)
            ? Directory.EnumerateFiles(dir,"*.SFD",SearchOption.TopDirectoryOnly)
                .Count(p=>MovieResolver.IsMapped(Path.GetFileNameWithoutExtension(p)))
            : 0;
        Console.WriteLine($"[movie] Dreamcast movies: {found} of {MovieResolver.MappedCount} in {dir}; any without an .SFD there play the PS1 version");
    }

    static bool Pressed => (Controller.State & (Controller.Start|Controller.Cross)) != (Controller.Start|Controller.Cross);

    public static MovieOverrideResult TryPlay(string ps1Name)
    {
        if(Active || HostWindow.IsHeadless || Environment.GetEnvironmentVariable("SPIDEY_DREAMCAST_MOVIES")=="0")
            return MovieOverrideResult.Unavailable;
        string dir=Folder;
        bool began=false;
        try
        {
            string? path=MovieResolver.Resolve(ps1Name,dir);
            if(path==null)
            {
                string? stem=MovieResolver.MappedStem(ps1Name);
                Console.WriteLine(stem==null
                    ? $"[movie] {ps1Name}: no Dreamcast version; PS1 movie"
                    : $"[movie] {ps1Name}: no {stem}.SFD in {dir}; PS1 movie");
                return MovieOverrideResult.Unavailable;
            }
            using var sfd=new NativeSfd(path);
            byte[] rgba=new byte[checked(sfd.Width*sfd.Height*4)];
            if(!sfd.ReadFrameRgba(rgba)) throw new InvalidDataException("SFD contains no complete video frame");
            // Do not touch host audio/display until the original SFD header, ADX
            // header and first display-order MPEG picture have actually decoded.
            using var audio=Audio.BeginMovie(sfd);
            Active=true; began=true;
            // Disposed before audio: old XA is cleared before the original mixer
            // resumes. Native cleanup/fallback/reset releases the STR producer.
            using var streamHold=Sdk.LibCdStream.HoldForHostMovie();
            var timing=new MovieTiming(Pressed);
            int last=0, shown=1;
            HostWindow.SetMovieFrame(rgba,sfd.Width,sfd.Height,sfd.DisplayAspect);
            Console.WriteLine($"[movie] live SFD decoder {ps1Name} <- {Path.GetFileName(path)}; " +
                $"{sfd.Width}x{sfd.Height} {sfd.FpsNumerator}/{sfd.FpsDenominator} fps; {sfd.SampleRate} Hz; " +
                $"clock={(audio.HasDevice?"OpenAL":"monotonic (audio unavailable)")}; no converted cache");
            while(true)
            {
                audio.Pump();
                double seconds=audio.PositionSeconds;
                if(timing.Skip(seconds,Pressed)) { Console.WriteLine("[movie] skipped by mapped Start/Cross"); break; }
                // Streaming, not random access. Retain reference frames natively,
                // decode/drop intermediate display frames when the host is late.
                // Bound catch-up per event-pump iteration to keep reset/skip live.
                int wanted=MovieTiming.FrameAt(seconds,sfd.FpsNumerator,sfd.FpsDenominator,NativeSfd.MaximumFrames);
                bool changed=false;
                for(int work=0;work<8 && !sfd.VideoEnded && last<wanted;work++)
                {
                    if(!sfd.ReadFrameRgba(rgba)) break;
                    last=sfd.DecodedFrames-1; changed=true;
                    if((work&3)==3) audio.Pump();
                }
                if(changed) { HostWindow.SetMovieFrame(rgba,sfd.Width,sfd.Height,sfd.DisplayAspect); shown++; }
                if(sfd.VideoEnded && seconds>=Math.Max(sfd.DecodedVideoDuration,sfd.AudioDuration)) break;
                // If the safety frame limit is reached, explicitly request EOF;
                // never get stuck forever on the clamped final frame index.
                if(sfd.DecodedFrames==NativeSfd.MaximumFrames && !sfd.VideoEnded && seconds>=sfd.DecodedVideoDuration)
                {
                    if(sfd.ReadFrameRgba(rgba)) throw new InvalidDataException("SFD exceeds supported duration");
                }
                // Real interrupt, pad, reset and host event path remains in use.
                Runtime.PresentFrame();
            }
            Console.WriteLine($"[movie] native cleanup; {sfd.DecodedFrames} decoded/{shown} presentations; original SFD input");
            return MovieOverrideResult.Played;
        }
        catch(Exception e) when(e is IOException or InvalidDataException or UnauthorizedAccessException or
            ArgumentException or OverflowException or InvalidOperationException or DllNotFoundException or
            EntryPointNotFoundException or BadImageFormatException)
        {
            Console.Error.WriteLine($"[movie] rejected {ps1Name}: {e.Message}; PS1 fallback");
            return began ? MovieOverrideResult.Failed : MovieOverrideResult.Unavailable;
        }
        finally
        {
            if(began) { HostWindow.ClearMovieFrame(); FrameClock.Resync(); }
            Active=false;
        }
    }
}
