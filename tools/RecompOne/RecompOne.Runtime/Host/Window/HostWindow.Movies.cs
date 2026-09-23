using Silk.NET.OpenGL;
using RecompOne.Runtime.Host.Window;

namespace RecompOne.Runtime.Host;

public static partial class HostWindow
{
    static uint _movieTex;
    static byte[]? _movieRgba;
    static int _movieWidth,_movieHeight,_movieTexWidth,_movieTexHeight;
    static float _movieAspect;
    static bool _movieDirty;
    static bool MovieFrameActive => _movieRgba != null;

    /// <summary>Called on the same game/render thread; the caller owns this buffer.</summary>
    internal static void SetMovieFrame(byte[] rgba,int width,int height,float aspect)
    {
        if(width<=0 || height<=0 || width>1024 || height>1024 || rgba.Length!=width*height*4 || !float.IsFinite(aspect) || aspect<=0)
            throw new ArgumentException("invalid movie display frame");
        _movieRgba=rgba; _movieWidth=width; _movieHeight=height; _movieAspect=aspect; _movieDirty=true;
    }
    internal static void ClearMovieFrame() { _movieRgba=null; _movieDirty=false; }
    static void UploadMovieTexture(GL gl)
    {
        if(_movieRgba==null) return;
        if(_movieTex==0)
        {
            _movieTex=CreateTexture(gl);
            gl.TexParameter(TextureTarget.Texture2D,TextureParameterName.TextureMinFilter,(int)GLEnum.Linear);
            gl.TexParameter(TextureTarget.Texture2D,TextureParameterName.TextureMagFilter,(int)GLEnum.Linear);
        }
        if(_movieDirty)
        {
            gl.BindTexture(TextureTarget.Texture2D,_movieTex);
            if(_movieTexWidth!=_movieWidth || _movieTexHeight!=_movieHeight)
            {
                gl.TexImage2D<byte>(TextureTarget.Texture2D,0,InternalFormat.Rgba,(uint)_movieWidth,(uint)_movieHeight,
                    0,PixelFormat.Rgba,PixelType.UnsignedByte,_movieRgba.AsSpan());
                _movieTexWidth=_movieWidth; _movieTexHeight=_movieHeight;
            }
            else gl.TexSubImage2D<byte>(TextureTarget.Texture2D,0,0,0,(uint)_movieWidth,(uint)_movieHeight,
                PixelFormat.Rgba,PixelType.UnsignedByte,_movieRgba.AsSpan());
            _movieDirty=false;
        }
        OutputPanel.SetTexture(_movieTex,_movieWidth,_movieHeight,_movieAspect);
    }
}
