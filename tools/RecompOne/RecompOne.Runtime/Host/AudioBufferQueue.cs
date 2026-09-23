using Silk.NET.OpenAL;

namespace RecompOne.Runtime.Host;

/// <summary>Streaming refill and underrun recovery, shared by both games.</summary>
internal static unsafe class AudioBufferQueue
{
    internal readonly record struct Observation(int State, int Processed, bool Recovered);

    internal static Observation Refill<T>(AL al, uint source, uint[] buffers,
        short[] samples, int frames, T producer, Action<T, short[], int> mix)
    {
        al.GetSourceProperty(source, GetSourceInteger.SourceState, out int state);
        al.GetSourceProperty(source, GetSourceInteger.BuffersProcessed, out int processed);
        int processedAtEntry = processed;
        if (state == (int)SourceState.Stopped)
        {
            Recover();
            return new(state, processedAtEntry, true);
        }

        while (processed-- > 0)
        {
            uint buffer;
            al.SourceUnqueueBuffers(source, 1, &buffer);
            Fill(buffer);
            al.SourceQueueBuffers(source, 1, &buffer);
        }
        al.GetSourceProperty(source, GetSourceInteger.SourceState, out state);
        if (state == (int)SourceState.Stopped)
        {
            // Playback can drain while Mix or the caller is descheduled. Play on
            // this queue would restart its already-consumed buffers from the front.
            Recover();
            return new(state, processedAtEntry, true);
        }
        if (state != (int)SourceState.Playing) al.SourcePlay(source);
        return new(state, processedAtEntry, false);

        void Fill(uint buffer)
        {
            mix(producer, samples, frames);
            al.BufferData(buffer, BufferFormat.Stereo16, samples, 44100);
        }

        void Recover()
        {
            // A stopped source cannot consume this rebuilt queue until Play.
            // Discard the ambiguous old queue, including any just-mixed audio
            // that may already have played during a mid-refill scheduling gap.
            // This can discard at most one queue of fresh samples; never replay it.
            al.SetSourceProperty(source, SourceInteger.Buffer, 0);
            foreach (uint buffer in buffers) Fill(buffer);
            fixed (uint* all = buffers) al.SourceQueueBuffers(source, buffers.Length, all);
            al.SourcePlay(source);
        }
    }
}
