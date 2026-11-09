/*
 * The suit rig engine (src/rig.c), shared by both games: the PC's NativeRetargetRig.
 */
#pragma once
#include <stdint.h>

/* Load the suit's RTG2 rig and check the unpacked evaluation against the core. 1: loaded. */
int rig_install(const char *path);   /* drops any rig loaded before; NULL: none */
void rig_unload(void);
int rig_loaded(void);
/* Mesh pages the rig drives (18 native parts, more for paged actors). */
int rig_pages(void);
/* The resident mesh of a page, as found by the last rig_apply. */
uint32_t rig_mesh(int page);
/* The 18 native drivers from a pose (18 x 24 bytes) as 3x4 float matrices. */
void rig_read_drivers(uint32_t pose, float *drivers);
/* Pose, skin and write the suit into the resident meshes listed at table (pages
 * pointers, the count at table-4). Everything is validated before anything is written.
 * 1: written. */
int rig_apply(uint32_t table, int pages, const float *drivers);
/* The swing web: the rig's wrist in world space, and one segment bent onto it. 1: ok. */
int rig_web_target(const float *drivers, unsigned part, const short *rotation, const int *translation,
                   const int *position, unsigned mirror, int *out);
int rig_web_segment(const int *start, const int *end, const int *tip, const int *target, unsigned index, unsigned count, int *out);
int rig_ram(uint32_t p, uint32_t n);
