namespace RecompOne.Runtime.Events;

/// <summary>Prepare process-local input for the upcoming console tick, before pad sampling.</summary>
public sealed class VSyncInputEvent : GameEvent
{
    public long Frame;
}
