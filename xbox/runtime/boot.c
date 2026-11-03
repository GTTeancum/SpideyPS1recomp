/*
 * Boot, following the generated PC Entry.Run: load the boot executable's text into
 * RAM at its load address, make the main program callable, set GP/SP/FP and run the
 * configured main function (or the executable's entry point).
 */
#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"

static CpuContext g_cpu;
static int g_cpu_ready;

CpuContext *rt_cpu(void) { return g_cpu_ready ? &g_cpu : 0; }

void rt_boot(const RecompGame *game, const char *disc_root)
{
    char path[260];
    snprintf(path, sizeof path, "%s\\%s", disc_root, game->boot_exe);
    FILE *f = fopen(path, "rb");
    if (!f) { rt_log("cannot open %s", path); return; }
    fseek(f, 0x800, SEEK_SET);
    uint32_t base = game->load_address & (RECOMP_RAM_SIZE - 1u);
    size_t read = fread(recomp_ram + base, 1, game->text_size, f);
    fclose(f);
    if (read != game->text_size) {
        rt_log("short read of %s: %u of %lu bytes", path, (unsigned)read, (unsigned long)game->text_size);
        return;
    }
    rt_log("%s: %lu bytes at %08lX", game->boot_exe, (unsigned long)game->text_size, (unsigned long)game->load_address);
    if (!cd_init(disc_root)) { rt_log("no disc map; CD reads will fail"); }

    rt_dispatch_init(game);
    bios_init();
    if (!rt_overlay_load("main")) { rt_log("no main overlay"); return; }

    CpuContext *c = &g_cpu;
    c->r[28] = game->gp;
    c->r[29] = game->sp;
    c->r[30] = game->sp;
    c->r[31] = 0;
    g_cpu_ready = 1;
    rt_clock_init();
    rt_log("starting %s", game->title);
    if (game->main) game->main(c);
    else recomp_dispatch(c, game->pc);
    rt_log("%s returned", game->title);
}
