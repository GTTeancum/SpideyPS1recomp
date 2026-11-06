/*
 * Movies the runtime plays itself, in place of the game's own STR player (there is no
 * MDEC or XA streaming on the Xbox): the original PlayStation .STR (str.c) or a
 * Dreamcast .SFD (the PC's decoder, sfd_core.cpp). Sound goes out of the AC97 at
 * 48 kHz, pictures go to the NV2A (hw_picture_*), and the console clock keeps
 * ticking, so the game's interrupts and the pads run as they do in a frame. Start or
 * Cross skips, as on the PC (a fresh press, after the first second).
 */
#pragma once

/* 1: played to the end or skipped. 0: the file is missing or cannot be decoded, or
 * playback failed; nothing is left on screen or in the sound output. */
int movie_play_str(const char *path);
int movie_play_sfd(const char *path);
