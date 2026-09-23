namespace RecompOne.Runtime.Media;

/// <summary>Pure presentation/skip decisions, also linked by BCL-only regression.</summary>
public sealed class MovieTiming
{
    bool _released;
    bool _previous;
    public MovieTiming(bool initiallyPressed) { _previous=initiallyPressed; _released=!initiallyPressed; }
    public bool Skip(double seconds,bool pressed)
    {
        if (!pressed) _released=true;
        bool result=seconds>=1 && _released && pressed && !_previous;
        _previous=pressed; return result;
    }
    public static int FrameAt(double seconds,uint fpsNumerator,uint fpsDenominator,int count)
    {
        if (!double.IsFinite(seconds) || seconds<0 || fpsNumerator==0 || fpsDenominator==0 || count<=0)
            throw new ArgumentOutOfRangeException(nameof(seconds));
        return (int)Math.Min(count-1,Math.Floor(seconds*fpsNumerator/fpsDenominator));
    }
}
