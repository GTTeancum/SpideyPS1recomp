// The PC's Dreamcast SFD decoder (tools/movies/native/sfd_decoder.cpp: MPEG-1 video,
// ADX sound), compiled unchanged into the Xbox build. It has no CRT, allocation or
// platform dependency. Its _fltused is renamed: the retarget core defines one too.
#define _fltused sfd_fltused
#include "../../tools/movies/native/sfd_decoder.cpp"
