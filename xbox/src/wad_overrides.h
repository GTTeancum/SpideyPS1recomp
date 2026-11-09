/*
 * CD.WAD entries served from files (src/wad_overrides.c, the PC's LooseWadOverrides),
 * shared by both games.
 */
#pragma once
#include "recomp.h"

/* Reset the arena and read D:\assets\builtin\index.txt (at start-up). */
void wad_init(void);
/* Whether D:\assets\builtin holds this file. */
int wad_have(const char *name);

/* A lookup for `name`, served from D:\assets\builtin\<alias> (alias NULL: the same
 * name) when that file is there. 1: pending. An alias the disc's WAD may not hold is
 * completed with wad_complete_aliased. */
int wad_find(const char *name, const char *alias);
/* A lookup served from any file (a suit's own model). Always aliased. */
int wad_find_file(const char *path);
int wad_pending(void);
/* Post CdWadFind: the entry's size is the pending file's (rounded to sectors). */
void wad_find_exit(CpuContext *c);
/* Pre CdWadFind, for an aliased lookup: answer it without the disc. 1: answered. */
int wad_complete_aliased(CpuContext *c);
/* Pre HeapAlloc: the allocation for the pending file comes from the arena. 1: served. */
int wad_heap_alloc(CpuContext *c);
/* Pre CdWadRead: read the pending file into the buffer. 1: served. */
int wad_read(CpuContext *c);

/* The arena: bounded scratch (0 when exhausted), its blocks, and freeing one. */
uint32_t wad_scratch(uint32_t size);
int wad_allocation(uint32_t address, uint32_t *size);
int wad_owns(uint32_t address);
int wad_free(uint32_t address);
