// OpenSpidey original-SFD streaming decoder; fixed, caller-owned workspace.
// ABI 0x00010000. All calls are serial per workspace. No file writes, processes,
// allocations, OS imports or dependency on an installed codec package.
#pragma once
#if defined(_WIN32)
#define SFD_API extern "C" __declspec(dllexport)
#define SFD_CALL __cdecl
#else
#define SFD_API extern "C" __attribute__((visibility("default")))
#define SFD_CALL
#endif
using SfdU8 = unsigned char;
using SfdU32 = unsigned int;
using SfdU64 = unsigned long long;
// Return exactly the number read, 0 for EOF, negative for I/O failure. Never throw
// across this callback. The owner keeps the SAME read-only file handle alive.
using SfdReadAt = int(SFD_CALL *)(void* user, SfdU64 offset, SfdU8* destination, SfdU32 length);
struct SfdInfo {
 SfdU32 width, height, fps_num, fps_den, sar_num, sar_den;
 SfdU32 sample_rate, channels, audio_frames, reserved;
};
// negative results are permanent decoder errors; 0 EOF; positive produced data.
// Info/open returns 0 success. Workspace pointer must be 16-byte aligned.
SFD_API SfdU32 sfd_abi();
SFD_API SfdU32 sfd_workspace_size();
SFD_API int sfd_open(void* workspace, SfdU32 bytes, SfdReadAt read, void* user,
                    SfdU64 file_size, SfdInfo* info);
// One display-order frame in tight YUV420 (Y then U then V). frame_index starts 0.
SFD_API int sfd_video(void* workspace, SfdU8* yuv, SfdU32 capacity, SfdU32* frame_index);
// Original-rate stereo PCM16, mono input duplicated. Returns frames, not shorts.
SFD_API int sfd_audio(void* workspace, short* pcm, SfdU32 capacity_frames);
// Convert a tightly packed YUV420 frame to RGBA8 (BT.601 limited range).
SFD_API int sfd_rgba(const SfdU8* yuv, SfdU32 yuv_bytes, SfdU32 width, SfdU32 height,
                    SfdU8* rgba, SfdU32 capacity);
SFD_API int sfd_error(void* workspace);
SFD_API void sfd_close(void* workspace);
