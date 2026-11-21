using System.Numerics;

namespace Recompiled;

/// <summary>USA N64 800378B0 projection; verified by executing the ROM routine.</summary>
public static class N64ShadowProjection
{
    public static Matrix4x4 Create(Vector3 anchor, Vector3 normal)
    {
        // 800BDF18 builds a basis from -normal, initially using world X.
        // 800BDDB8 then zeroes its Y row, and 800BDD5C translates by -anchor.
        // This is a rotated, flattened model, not a directional-light projection.
        Vector3 up = -normal;
        Vector3 forward = NormalizeOrZero(Vector3.Cross(Vector3.UnitX, up));
        Vector3 right = NormalizeOrZero(Vector3.Cross(up, forward));
        Vector3 translation = anchor - anchor.X * right - anchor.Z * forward;
        return new Matrix4x4(
            right.X, right.Y, right.Z, 0,
            0, 0, 0, 0,
            forward.X, forward.Y, forward.Z, 0,
            translation.X, translation.Y, translation.Z, 1);
    }

    static Vector3 NormalizeOrZero(Vector3 v) => v.LengthSquared() > 0 ? Vector3.Normalize(v) : Vector3.Zero;
}
