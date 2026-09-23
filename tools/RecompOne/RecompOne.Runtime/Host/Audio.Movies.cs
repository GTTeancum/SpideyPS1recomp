using System.Diagnostics;
using RecompOne.Runtime.Media;
using Silk.NET.OpenAL;

namespace RecompOne.Runtime.Host;

internal static unsafe partial class Audio
{
    static volatile bool _movieRequested;
    static readonly ManualResetEventSlim _movieSuspended = new(false);

    internal static MovieAudioSession BeginMovie(NativeSfd sfd)
    {
        if (!_running || _al == null) return new MovieAudioSession(sfd,null);
        if (_movieRequested) throw new InvalidOperationException("nested movie audio");
        // An interrupted previous session must finish its resume handshake before
        // this session requests a new suspension acknowledgement.
        var resume = Stopwatch.StartNew();
        while (_movieSuspended.IsSet && _running && resume.ElapsedMilliseconds < 2000) Thread.Sleep(1);
        if (_movieSuspended.IsSet || !_running) throw new IOException("previous movie audio did not resume");
        _movieRequested=true;
        try
        {
            if (!_movieSuspended.Wait(2000) || !_running) throw new IOException("audio mixer did not suspend");
            _alc!.MakeContextCurrent(_context);
            return new MovieAudioSession(sfd,_al);
        }
        catch { _movieRequested=false; throw; }
    }

    /// <summary>Separate bounded source; queued SPU/XA samples never mix with SFD PCM.</summary>
    internal sealed class MovieAudioSession : IDisposable
    {
        const int Chunk=2048;
        readonly NativeSfd _sfd;
        readonly AL? _api;
        readonly uint[] _ids = new uint[4];
        readonly short[] _pcm = new short[Chunk*2];
        readonly Queue<(uint Id,int Frames)> _queue = new();
        readonly Stopwatch _silent = new();
        readonly Stopwatch _stall = new();
        readonly Stopwatch _tail = new();
        uint _movieSource;
        long _completed, _submitted;
        double _position;
        bool _disposed;
        public bool HasDevice => _api != null;
        public double PositionSeconds => _api==null ? _silent.Elapsed.TotalSeconds :
            (_tail.IsRunning ? _sfd.AudioDuration+_tail.Elapsed.TotalSeconds : _position);

        internal MovieAudioSession(NativeSfd sfd, AL? api)
        {
            _sfd=sfd; _api=api;
            if(api==null) { _silent.Start(); return; }
            try
            {
                // Ignore unrelated historical errors; all following operations belong
                // exclusively to this suspended-mixer movie session.
                api.GetError();
                _movieSource=api.GenSource();
                fixed(uint* p=_ids) api.GenBuffers(_ids.Length,p);
                api.SetSourceProperty(_movieSource,SourceFloat.Gain,_masterVolume);
                foreach(uint id in _ids) Queue(id);
                if (_queue.Count==0) throw new InvalidDataException("empty movie PCM");
                api.SourcePlay(_movieSource); CheckError(); _stall.Start();
                Console.WriteLine($"[movie] audio {_sfd.SampleRate} Hz stereo; device clock");
            }
            catch { Dispose(); throw; }
        }

        void Queue(uint id)
        {
            int frames=_sfd.ReadAudio(_pcm,Chunk);
            if(frames==0) return;
            fixed (short* samples = _pcm)
                _api!.BufferData(id,BufferFormat.Stereo16,samples,frames*2*sizeof(short),_sfd.SampleRate);
            uint buffer=id; _api.SourceQueueBuffers(_movieSource,1,&buffer);
            _queue.Enqueue((id,frames)); _submitted+=frames;
        }

        void CheckError()
        {
            if (_api!.GetError() != AudioError.NoError) throw new IOException("movie OpenAL operation failed");
        }

        public void Pump()
        {
            if(_api==null)
            {
                // Still validate/decode the original ADX stream when silent. Keep
                // work bounded to playback progress plus the normal queue lead.
                long wanted=Math.Min(_sfd.AudioFrames,(long)(_silent.Elapsed.TotalSeconds*_sfd.SampleRate)+Chunk*4);
                while(_submitted<wanted)
                {
                    int frames=_sfd.ReadAudio(_pcm,Chunk);
                    if(frames==0) break;
                    _submitted+=frames;
                }
                return;
            }
            if(_tail.IsRunning) return;
            _api.SetSourceProperty(_movieSource,SourceFloat.Gain,_masterVolume);
            _api.GetSourceProperty(_movieSource,GetSourceInteger.BuffersProcessed,out int processed);
            if(processed<0 || processed>_queue.Count) throw new IOException("invalid movie audio queue state");
            var refill = new List<uint>(4);
            for(int i=0;i<processed;i++)
            {
                uint id; _api.SourceUnqueueBuffers(_movieSource,1,&id);
                var item=_queue.Dequeue();
                if(id!=item.Id) throw new IOException("movie audio queue order changed");
                _completed+=item.Frames; refill.Add(id);
            }
            _api.GetSourceProperty(_movieSource,GetSourceInteger.SourceState,out int state);
            // A drained source before all input was submitted is a real underrun.
            // Fail safely to PS1 rather than replaying already-consumed buffers.
            if(state==(int)SourceState.Stopped && _submitted<_sfd.AudioFrames)
                throw new IOException("movie audio underrun; restoring PS1 playback");
            foreach(uint id in refill) Queue(id);
            if(_completed==_sfd.AudioFrames && _queue.Count==0)
            { CheckError(); _position=_sfd.AudioDuration; _tail.Start(); return; }
            _api.GetSourceProperty(_movieSource,(GetSourceInteger)0x1025,out int offset); // AL_SAMPLE_OFFSET
            long queued=_submitted-_completed;
            CheckError();
            if(offset<0 || offset>queued) throw new IOException("invalid movie audio cursor");
            double next=(double)(_completed+offset)/_sfd.SampleRate;
            if(next>_position) { _position=next; _stall.Restart(); }
            else if(_stall.Elapsed.TotalSeconds>2) throw new IOException("movie audio device stopped advancing");
        }

        public void Dispose()
        {
            if(_disposed) return; _disposed=true;
            try
            {
                if(_api!=null)
                {
                    if(_movieSource!=0) { _api.SourceStop(_movieSource); _api.DeleteSource(_movieSource); }
                    uint[] allocated = _ids.Where(id => id != 0).ToArray();
                    if (allocated.Length != 0) _api.DeleteBuffers(allocated);
                }
            }
            finally
            {
                if(_api!=null)
                {
                    _movieRequested=false;
                    // Wait until the mixer leaves its suspension, so a following
                    // movie cannot accidentally reuse the previous acknowledgement.
                    var wait=Stopwatch.StartNew();
                    while(_running && _movieSuspended.IsSet && wait.ElapsedMilliseconds<2000) Thread.Sleep(1);
                }
            }
        }
    }
}
