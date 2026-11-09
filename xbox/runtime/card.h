/*
 * The PlayStation memory cards on the Xbox hard disk: a port of the PC runtime's
 * MemoryCard (tools/RecompOne/RecompOne.Runtime/Hardware/MemoryCard.cs) and its BIOS
 * card calls (Bios/BiosA.cs, Bios/BiosB.cs). Both slots are present, as on the PC:
 * 128 KB images in E:\UDATA\<title ID>\<save folder>\carda.sav and cardb.sav.
 */
#pragma once
#include <stdint.h>
#include "recomp.h"

/* Mount the hard disk and load (or format) the card image. */
void card_init(void);

/* BIOS calls: 1 if the function is a card call (handled), else 0. */
int card_bios_a(CpuContext *c, uint32_t fn);
int card_bios_b(CpuContext *c, uint32_t fn);

/* Once per tick: re-deliver pending card completions (the PC's BiosB.PumpCard). */
void card_pump(void);

/* The save folder (E:\UDATA\<title ID>\<save folder>) once the hard disk is mounted,
 * else NULL. */
const char *card_dir(void);
