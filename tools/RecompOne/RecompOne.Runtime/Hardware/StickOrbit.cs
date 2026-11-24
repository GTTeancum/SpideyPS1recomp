namespace RecompOne.Runtime.Hardware;

/// <summary>Horizontal orbit in the game's 4096-unit circle, timed by 60 Hz vblanks.</summary>
public sealed class StickOrbit(double degreesPerSecond = 90)
{
    const int DeadZone = 24;
    readonly double _unitsPerVBlank = degreesPerSecond * (4096.0 / 360 / 60);
    uint _owner;
    long _frame;
    double _yaw;
    bool _active;

    public void Reset() => _active = false;

    public bool Update(byte axis, long frame, uint owner, ushort currentYaw, out ushort yaw)
    {
        yaw = currentYaw;
        // Both 127 and 128 are neutral; give the two directions identical ranges.
        int x = axis < 128 ? axis - 127 : axis - 128;
        int magnitude = Math.Abs(x) - DeadZone;
        if (magnitude <= 0)
        {
            Reset();
            return false;
        }

        long elapsed = frame - _frame;
        if (!_active || owner != _owner || elapsed < 0 || elapsed > 6)
        {
            _yaw = currentYaw & 4095;
            elapsed = 2; // A normal game update; never catch up across a pause/load.
        }
        _owner = owner;
        _frame = frame;
        _active = true;
        _yaw = (_yaw + Math.Sign(x) * magnitude / (127.0 - DeadZone) *
            _unitsPerVBlank * elapsed) % 4096;
        if (_yaw < 0) _yaw += 4096;
        yaw = (ushort)((int)Math.Round(_yaw) & 4095);
        return true;
    }
}
