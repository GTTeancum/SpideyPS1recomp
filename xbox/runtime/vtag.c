/*
 * Vertex tags in RAM (see recomp.h): the Xbox counterpart of the PC's GteScreen RAM
 * provenance (tools/RecompOne/RecompOne.Runtime/Hardware/GteScreen.cs). A tag per word,
 * in pages allocated the first time a tagged word lands in them: the game's packet
 * buffers, its mesh staging areas and the scratchpad.
 */
#include <stdlib.h>
#include "runtime.h"

VTag *vtag_pages[VTAG_PAGES];
volatile uint32_t rt_vtag_pages, rt_vtag_stores, rt_vtag_refused;
void (*rt_vtag_store_hook)(uint32_t address, uint32_t value, VTag *tag);

/* 20 KB a page: 128 pages is 2.5 MB, more than the packet and staging buffers use. A
 * word beyond it goes untagged (the status counts them) rather than run memory out. */
#define PAGE_LIMIT 128

static VTag *page_for(uint32_t a)
{
    uint32_t p = a & 0x1FFFFFFFu, page;
    if (p < RECOMP_RAM_SIZE) page = p >> 12;
    else if ((p & ~0x3FFu) == 0x1F800000u) page = VTAG_PAGES - 1;
    else return 0;
    if (!vtag_pages[page]) {
        if (rt_vtag_pages >= PAGE_LIMIT) return 0;
        vtag_pages[page] = calloc(1024, sizeof(VTag));
        if (!vtag_pages[page]) return 0;
        rt_vtag_pages++;
    }
    return vtag_pages[page] + ((p & 4095u) >> 2);
}

/*
 * A tag only goes with a word that is its coordinate or a piece of it: the packed SXY
 * word itself, or one coordinate unpacked or with flags beside it (the PC keeps tags
 * through such scratch encodings and validates at the GPU, as this does). Anything else
 * is a register whose tag outlived its value through a hook or BIOS call that wrote it.
 */
static int related(uint32_t v, uint32_t native)
{
    uint32_t nx = native & 0x7FFu, ny = (native >> 16) & 0x7FFu;
    return (v & 0x07FF07FFu) == (native & 0x07FF07FFu) || (v & 0x7FFu) == nx || (v & 0x7FFu) == ny ||
           ((v >> 16) & 0x7FFu) == ny;
}

void vtag_store(uint32_t address, uint32_t value, const VTag *tag)
{
    if (!related(value, tag->native)) {
        VTag *e = vtag_slot(address);
        if (e) e->z = 0.0f;
        return;
    }
    VTag copy = *tag;
    copy.value = value;
    if (rt_vtag_store_hook) rt_vtag_store_hook(address & 0x1FFFFFFFu, value, &copy);
    VTag *e = page_for(address);
    if (!e) { rt_vtag_refused++; return; }
    *e = copy;
    rt_vtag_stores++;
}

__attribute__((noinline)) const VTag *vtag_load(uint32_t address, uint32_t value) { return vtag_of(address, value); }

__attribute__((noinline)) const VTag *vtag_containing(uint32_t address)
{
    const VTag *e = vtag_slot(address & ~3u);
    return e && e->z > 0.0f && e->value == mem_r32(address & ~3u) ? e : 0;
}

__attribute__((noinline)) void vtag_stored(uint32_t address, uint32_t value, const VTag *tag)
{
    if (tag) { vtag_store(address, value, tag); return; }
    VTag *e = vtag_slot(address);
    if (e) e->z = 0.0f;
}

__attribute__((noinline)) const VTag *vtag_pick(const VTag *a, const VTag *b)
{
    if (!a) return b;
    if (!b || a == b) return a;
    return a->native == b->native && a->x == b->x && a->y == b->y && a->z == b->z ? a : 0;
}
