// The PC's Dreamcast SFD decoder (tools/movies/native/sfd_decoder.cpp: MPEG-1 video,
// ADX sound), compiled unchanged into the Xbox build. It has no CRT, allocation or
// platform dependency. Its _fltused is renamed: the retarget core defines one too.
//
// Commented out (2026-11-09): Dreamcast movies are out on the Xbox for now. On the
// console the decoder plays at about 40% speed (the 14 s Activision logo took 35 s);
// the PS1 movies play in time. Restore with runtime/movie.c and src/movies_sm1.c.
#if 0
#define _fltused sfd_fltused
#include "../../tools/movies/native/sfd_decoder.cpp"
#endif
