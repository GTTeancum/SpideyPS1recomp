using System.Runtime.InteropServices;
using RecompOne.Runtime.Host;
using Silk.NET.OpenAL;

unsafe class Program
{
    static void Check(bool value, string label)
    {
        if (!value) throw new Exception(label);
        Console.WriteLine("PASS " + label);
    }

    static void Main()
    {
        using var alc = ALContext.GetApi(true);
        using var al = AL.GetApi(true);
        var open = (delegate* unmanaged[Cdecl]<byte*, Device*>)alc.GetProcAddress(null, "alcLoopbackOpenDeviceSOFT");
        var render = (delegate* unmanaged[Cdecl]<Device*, void*, int, void>)alc.GetProcAddress(null, "alcRenderSamplesSOFT");
        Check(open != null && render != null, "OpenAL Soft loopback available; no speaker device opened");
        Device* device = open(null);
        int* attributes = stackalloc int[] { 0x1007, 44100, 0x1990, 0x1501, 0x1991, 0x1406, 0 };
        var context = alc.CreateContext(device, attributes);
        Check(context != null && alc.MakeContextCurrent(context), "silent stereo float loopback context");
        try
        {
            foreach (string scenario in new[] { "normal", "stopped-entry", "mid-refill" })
            {
                uint source = al.GenSource();
                uint[] buffers = new uint[8];
                fixed (uint* p = buffers) al.GenBuffers(buffers.Length, p);
                short[] samples = new short[512];
                float[] output = new float[4096];
                void Render(int frames)
                {
                    fixed (float* p = output) render(device, p, frames);
                }
                int Property(GetSourceInteger property)
                {
                    al.GetSourceProperty(source, property, out int value);
                    return value;
                }
                try
                {
                    for (int i = 0; i < buffers.Length; i++)
                    {
                        Array.Fill(samples, (short)((i+1)*1000));
                        al.BufferData(buffers[i], BufferFormat.Stereo16, samples, 44100);
                    }
                    fixed (uint* p = buffers) al.SourceQueueBuffers(source, buffers.Length, p);
                    al.SourcePlay(source);
                    Render(scenario == "stopped-entry" ? 2048 : 256);
                    Check(Property(GetSourceInteger.BuffersProcessed) == (scenario == "stopped-entry" ? 8 : 1),
                        scenario + " has expected initial queue consumption");
                    int produced = 0;
                    Action<int, short[], int> produce = (_, destination, _) =>
                    {
                        if (scenario == "mid-refill" && produced == 0)
                        {
                            // Advance only this loopback device while the refill
                            // is in flight: deterministic scheduling gap equivalent.
                            Render(2048);
                            Check(Property(GetSourceInteger.SourceState) == (int)SourceState.Stopped,
                                "mid-refill queue drains before fresh buffer is queued");
                        }
                        Array.Fill(destination, (short)(9000 + produced++ * 1000));
                    };
                    var result = AudioBufferQueue.Refill(al, source, buffers, samples, 256, 0, produce);
                    Check(result.Recovered == (scenario != "normal"), scenario + " recovery classification");
                    Check(Property(GetSourceInteger.SourceState) == (int)SourceState.Playing,
                        scenario + " source resumes playing");
                    Check(Property(GetSourceInteger.BuffersQueued) == 8, scenario + " keeps bounded eight-buffer queue");
                    Check(produced == (scenario == "normal" ? 1 : scenario == "stopped-entry" ? 8 : 9),
                        scenario + " mixes only expected buffers");
                    Render(256);
                    var sorted = output.Take(512).Order().ToArray();
                    float median = sorted[256];
                    float expected = (scenario == "normal" ? 2000 : scenario == "stopped-entry" ? 9000 : 10000) / 32768f;
                    Check(Math.Abs(median-expected) < .002f,
                        scenario + " renders expected PCM, never restarts consumed buffer 2000 on recovery");
                    Check(al.GetError() == AudioError.NoError, scenario + " has no OpenAL errors");

                    // Hold loopback time still to check the common no-refill path
                    // without attributing driver allocations to source generation.
                    AudioBufferQueue.Refill(al, source, buffers, samples, 256, 0, produce);
                    for (int i = 0; i < 100; i++) AudioBufferQueue.Refill(al, source, buffers, samples, 256, 0, produce);
                    long before = GC.GetAllocatedBytesForCurrentThread();
                    for (int i = 0; i < 1000; i++) AudioBufferQueue.Refill(al, source, buffers, samples, 256, 0, produce);
                    Check(GC.GetAllocatedBytesForCurrentThread() == before, scenario + " polling adds no managed allocations");
                }
                finally
                {
                    al.SourceStop(source);
                    al.DeleteSource(source);
                    al.DeleteBuffers(buffers);
                }
            }
        }
        finally
        {
            alc.MakeContextCurrent(null);
            alc.DestroyContext(context);
            alc.CloseDevice(device);
        }
    }
}
