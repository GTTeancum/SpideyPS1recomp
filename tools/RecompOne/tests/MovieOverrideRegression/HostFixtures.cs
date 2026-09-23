using RecompOne.Runtime.Media;
namespace Fixture
{
    public static class State
    {
        public static string Root="";
        public static double Seconds;
        public static int Presents, Uploaded, Opened, Disposed, Cleared, Resynced, StreamHeld, StreamDisposed;
        public static string Mode="";
        public static void Reset(string mode="")
        {
            Seconds=0; Presents=Uploaded=Opened=Disposed=Cleared=Resynced=StreamHeld=StreamDisposed=0; Mode=mode;
            RecompOne.Runtime.Hardware.Controller.State=0xFFFF;
        }
    }
    public sealed class ResetSignal : Exception { }
}
namespace RecompOne.Runtime.Hardware
{
    public static class Controller
    {
        public const ushort Start=1<<3, Cross=1<<14;
        public static ushort State=0xFFFF;
    }
}
namespace RecompOne.Runtime
{
    public static class Runtime
    {
        public static void PresentFrame()
        {
            Fixture.State.Presents++; Fixture.State.Seconds+=1.0/60;
            if(Fixture.State.Mode=="reset" && Fixture.State.Presents==5) throw new Fixture.ResetSignal();
            if(Fixture.State.Mode=="skip" && Fixture.State.Presents==80)
                Hardware.Controller.State=(ushort)(0xFFFF ^ Hardware.Controller.Cross);
        }
    }
}
namespace RecompOne.Runtime.Host
{
    public static class RuntimePaths { public static string ApplicationDirectory=>Fixture.State.Root; }
    public static class FrameClock { public static void Resync()=>Fixture.State.Resynced++; }
    public static class HostWindow
    {
        public static bool IsHeadless;
        public static void SetMovieFrame(byte[] rgba,int w,int h,float aspect)
        { if(rgba.Length!=w*h*4 || aspect<=0) throw new Exception("bad frame upload"); Fixture.State.Uploaded++; }
        public static void ClearMovieFrame()=>Fixture.State.Cleared++;
    }
    public static class Audio
    {
        public static Session BeginMovie(NativeSfd sfd)
        { if(Fixture.State.Mode=="begin-error") throw new IOException("injected audio start error"); Fixture.State.Opened++; return new Session(sfd); }
        public sealed class Session:IDisposable
        {
            readonly NativeSfd _sfd;
            readonly short[] _pcm=new short[4096];
            public Session(NativeSfd sfd) { _sfd=sfd; }
            public bool HasDevice=>false;
            public double PositionSeconds=>Fixture.State.Seconds;
            public void Pump()
            {
                if(Fixture.State.Mode=="audio-error" && Fixture.State.Presents==5) throw new IOException("injected audio device error");
                long target=Math.Min(_sfd.AudioFrames,(long)((Fixture.State.Seconds+0.15)*_sfd.SampleRate));
                while(_sfd.AudioFramesRead<target) if(_sfd.ReadAudio(_pcm,2048)==0) break;
            }
            public void Dispose()=>Fixture.State.Disposed++;
        }
    }
}

namespace RecompOne.Runtime.Sdk
{
    public static class LibCdStream
    {
        public static IDisposable HoldForHostMovie() { Fixture.State.StreamHeld++; return new Hold(); }
        sealed class Hold:IDisposable
        {
            public void Dispose()
            {
                if(Fixture.State.Disposed!=0) throw new Exception("stream/XA was not cleared before resuming audio");
                Fixture.State.StreamDisposed++;
            }
        }
    }
}
