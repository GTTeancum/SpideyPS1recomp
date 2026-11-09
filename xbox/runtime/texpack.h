/*
 * Texture packs: replacement art for textures the game uploads, found by the
 * fingerprint of the uploaded image and its palette. A port of the PC's page path in
 * RecompOne.Runtime.Assets.Textures (VramTracker uploads, TextureTile.Hash,
 * TextureResolver.ResolvePage, AssetReplacerManager.ResolveTexture); a fix to either
 * belongs in both places.
 *
 * The Dreamcast character models ship compact PlayStation textures that only identify
 * the art; the pack holds the art itself, as DXT1 DDS files made by
 * xbox/tools/prep_assets.py from the PC pack's PNGs.
 */
#pragma once
#include <stdint.h>
#include "runtime.h"

/* Load a pack's index (D:\...\index.txt). Returns the number of textures. */
int texpack_load(const char *dir);

/* VRAM bookkeeping: an image the CPU uploaded, and VRAM the GPU drew into (which no
 * longer holds an uploaded image). */
void texpack_note_upload(int x, int y, int w, int h);
void texpack_note_gpu_write(int x, int y, int w, int h);

/* The pack's art for the texels a primitive samples, if any. */
int texpack_lookup(const RtTile *tile, RtReplacement *out);

/* Once a frame: release loaded art not drawn lately while over the budget. */
void texpack_trim(void);
