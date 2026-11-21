using System.Numerics;

namespace Recompiled;

public enum ShadowScene { Ordinary, HideWhileAttached, PixelDepth, ZipLineTraining }

/// <summary>Visibility decisions from the N64 receiver selector and silhouette renderer.</summary>
public static class ShadowVisibility
{
    public static bool AcceptsReceiver(ShadowScene scene, uint state, bool attached, bool scriptHidden,
        bool receiver, bool material, uint materialFlags) =>
        receiver && !scriptHidden && state != 0x400 && (!material || (materialFlags & 1) == 0)
        && !(scene == ShadowScene.HideWhileAttached && attached);

    public static bool InitialPixelDepth(ShadowScene scene, uint state, Vector3 eye, Vector3 player) =>
        scene is ShadowScene.PixelDepth or ShadowScene.ZipLineTraining || (state & 0x00800480) != 0
        || Vector3.DistanceSquared(eye, player) > 1500f * 1500f;

    public static bool CameraHides(bool material, bool pixelDepth, float eyeY, float forwardY, float height) =>
        material && !pixelDepth && forwardY > 0.5f && eyeY > height;
}
