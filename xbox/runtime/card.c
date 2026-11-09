/*
 * The PlayStation memory card on the Xbox hard disk (card.h). The card image, its
 * directory frames and the BIOS file calls follow the PC runtime exactly
 * (Hardware/MemoryCard.cs, Bios/BiosA.cs, Bios/BiosB.cs), so a card file moves between
 * the PC and the Xbox unchanged. Both card slots are present, as on the PC (its
 * GameConfig enables both): carda.sav and cardb.sav.
 *
 * On disk: E:\UDATA\<title ID>\ holds TitleMeta.xbx (the name the dashboard shows) and
 * TitleImage.xbx; the save folder beside it holds SaveMeta.xbx, SaveImage.xbx and the
 * card image. The two images come from the disc (D:\TitleImage.xbx, D:\SaveImage.xbx,
 * made by tools/make_xbox_images.py) and are copied once.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <nxdk/mount.h>
#include "files.h"
#include "runtime.h"
#include "card.h"
#include "title.h"

#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define V0 c->r[2]

#define CARD_SIZE 0x20000
#define FR 0x80          /* frame */
#define BLK 0x2000       /* block */
#define DIRS 15          /* directory frames 1..15, one per block */

#define TITLE_DIR "E:\\UDATA\\" OPENSPIDEY_TITLE_ID_HEX
/* Dashboard save folders are twelve hex digits; this one is "OpenSp" in ASCII. */
#define SAVE_DIR TITLE_DIR "\\4F70656E5370"
static const char *const g_card_file[2] = { SAVE_DIR "\\carda.sav", SAVE_DIR "\\cardb.sav" };

/* The two card images; g_card is the one the current call addresses (use_card). */
static uint8_t g_cards[2][CARD_SIZE];
static uint8_t *g_card = g_cards[0];
static int g_cur;
static void use_card(int i) { g_cur = i & 1; g_card = g_cards[g_cur]; }
static int g_ready;                /* the hard disk and the card images are usable */
static uint32_t g_errno, g_last_chan;

volatile uint32_t rt_card_ready, rt_card_writes, rt_card_files, rt_card_saves;   /* status text */

/* ---- the card image (MemoryCard.cs) --------------------------------------------- */
static uint8_t frame_sum(int o) { uint8_t x = 0; for (int i = 0; i < 0x7F; i++) x ^= g_card[o + i]; return x; }
static void fix(int o) { g_card[o + 0x7F] = frame_sum(o); }

static void save_range(int offset, int length)
{
    if (!g_ready) return;
    /* At every save: not stdio (files.h). */
    if (rt_file_write(g_card_file[g_cur], (uint32_t)offset, g_card + offset, (uint32_t)length, 0)) rt_card_writes++;
}
static void save_all(void) { save_range(0, CARD_SIZE); }

/* Status text: how many saves (directory entries in use) the two cards hold. */
static void count_saves(void)
{
    uint32_t n = 0;
    for (int i = 0; i < 2; i++)
        for (int b = 1; b <= DIRS; b++) if (g_cards[i][b * FR] == 0x51) n++;
    rt_card_saves = n;
}

static void card_format(void)
{
    memset(g_card, 0, CARD_SIZE);
    g_card[0] = 0x4D; g_card[1] = 0x43; fix(0);
    for (int i = 1; i <= DIRS; i++) { int o = i * FR; g_card[o] = 0xA0; g_card[o + 8] = 0xFF; g_card[o + 9] = 0xFF; fix(o); }
    for (int i = 16; i <= 35; i++) {
        int o = i * FR;
        g_card[o] = g_card[o + 1] = g_card[o + 2] = g_card[o + 3] = 0xFF;
        g_card[o + 8] = 0xFF; g_card[o + 9] = 0xFF;
        fix(o);
    }
    save_all();
}

static void name_of(int b, char out[21])
{
    int o = b * FR + 0x0A, n = 0;
    while (n < 20 && g_card[o + n]) { out[n] = (char)g_card[o + n]; n++; }
    out[n] = 0;
}

static int file_size(int b)
{
    int o = b * FR + 4;
    return (int)(g_card[o] | g_card[o + 1] << 8 | g_card[o + 2] << 16 | (uint32_t)g_card[o + 3] << 24);
}

static int find_file(const char *name)
{
    char n[21];
    for (int b = 1; b <= DIRS; b++) {
        if (g_card[b * FR] != 0x51) continue;
        name_of(b, n);
        if (!strcmp(n, name)) return b;
    }
    return 0;
}

/* The blocks of a file, first to last; returns how many. */
static int chain(int first, int out[DIRS])
{
    int n = 0, b = first;
    while (b >= 1 && b <= DIRS && n < DIRS) {
        out[n++] = b;
        int next = g_card[b * FR + 8] | g_card[b * FR + 9] << 8;
        if (next == 0xFFFF) break;
        b = next + 1;
    }
    return n;
}

static int create_file(const char *name, int blocks)
{
    if (blocks < 1) blocks = 1;
    if (blocks > DIRS || find_file(name)) return 0;
    int free_blocks[DIRS], n = 0;
    for (int b = 1; b <= DIRS && n < blocks; b++) if (g_card[b * FR] == 0xA0) free_blocks[n++] = b;
    if (n < blocks) return 0;
    for (int i = 0; i < blocks; i++) {
        int b = free_blocks[i], o = b * FR;
        memset(g_card + o, 0, FR);
        g_card[o] = (uint8_t)(i == 0 ? 0x51 : i == blocks - 1 ? 0x53 : 0x52);
        int next = i == blocks - 1 ? 0xFFFF : free_blocks[i + 1] - 1;
        g_card[o + 8] = (uint8_t)next; g_card[o + 9] = (uint8_t)(next >> 8);
        if (i == 0) {
            uint32_t size = (uint32_t)blocks * BLK;
            g_card[o + 4] = (uint8_t)size; g_card[o + 5] = (uint8_t)(size >> 8);
            g_card[o + 6] = (uint8_t)(size >> 16); g_card[o + 7] = (uint8_t)(size >> 24);
            for (int k = 0; k < 20 && name[k]; k++) g_card[o + 0x0A + k] = (uint8_t)name[k];
        }
        fix(o);
        memset(g_card + b * BLK, 0, BLK);
    }
    save_all();
    return free_blocks[0];
}

static void delete_file(const char *name)
{
    int first = find_file(name), blocks[DIRS];
    if (!first) return;
    int n = chain(first, blocks);
    for (int i = 0; i < n; i++) { g_card[blocks[i] * FR] = 0xA0; fix(blocks[i] * FR); }
    save_all();
}

static int glob_match(const char *pat, const char *name)
{
    if (!*pat) return 1;
    for (; *pat; pat++, name++) {
        if (*pat == '*') return 1;
        if (!*name) return 0;
        if (*pat != '?' && *pat != *name) return 0;
    }
    return *name == 0;
}

/* ---- guest strings ----------------------------------------------------------------- */
static void guest_string(uint32_t p, char *out, int cap)
{
    int n = 0;
    while (n < cap - 1) { char ch = (char)mem_r8(p + (uint32_t)n); if (!ch) break; out[n++] = ch; }
    out[n] = 0;
}

/* "bu00:NAME" is card 1, "bu10:NAME" card 2: the name, with that card in use. */
static const char *card_path(const char *path)
{
    if (!g_ready) return 0;
    if ((path[0] == 'b' || path[0] == 'B') && (path[1] == 'u' || path[1] == 'U') && (path[2] == '0' || path[2] == '1') &&
        path[3] == '0' && path[4] == ':') {
        use_card(path[2] - '0');
        return path + 5;
    }
    return 0;
}

/* ---- completions (BiosB.CardComplete / PumpCard) ----------------------------------- */
#define PENDING 8
#define MAX_AGE 240      /* ticks a completion stays available for re-delivery */
static struct { uint32_t port; int age, used; } g_pending[PENDING];

static void complete(uint32_t port)
{
    g_last_chan = port;
    for (int i = 0; i < PENDING; i++)
        if (!g_pending[i].used) { g_pending[i].used = 1; g_pending[i].port = port; g_pending[i].age = 0; return; }
}

/* libmcrd starts an operation, then clears its event flags, then waits: a completion
 * handed over once is as likely to be wiped by that clear as seen, so it is delivered
 * again every tick until it ages out (the callbacks only set a flag). */
void card_pump(void)
{
    for (int i = 0; i < PENDING; i++) {
        if (!g_pending[i].used) continue;
        if (++g_pending[i].age > MAX_AGE) { g_pending[i].used = 0; continue; }
        uint32_t spec = 0x0004u;   /* both cards present */
        bios_deliver_event_intr(0xF4000001u, spec);
        bios_deliver_event_intr(0xF0000011u, spec);
    }
}

/* ---- open files (BiosA open/lseek/read/write/close) -------------------------------- */
#define FILES 4
static struct { int used, card, blocks[DIRS], count, size, pos; } g_files[FILES];
#define FD_BASE 2u

static int fd_slot(uint32_t fd) { uint32_t s = fd - FD_BASE; return s < FILES && g_files[s].used ? (int)s : -1; }

static void bios_open(CpuContext *c)
{
    char path[64];
    guest_string(A0, path, sizeof path);
    const char *name = card_path(path);
    if (!name) { V0 = 0xFFFFFFFFu; g_errno = 2; return; }
    int first = find_file(name);
    if (!first && (A1 & 0x200u)) first = create_file(name, (int)(A1 >> 16));
    if (!first) { V0 = 0xFFFFFFFFu; g_errno = 2; return; }
    for (int s = 0; s < FILES; s++)
        if (!g_files[s].used) {
            g_files[s].used = 1;
            g_files[s].card = g_cur;
            g_files[s].count = chain(first, g_files[s].blocks);
            g_files[s].size = file_size(first);
            g_files[s].pos = 0;
            V0 = FD_BASE + (uint32_t)s; g_errno = 0;
            rt_card_files++;
            return;
        }
    V0 = 0xFFFFFFFFu; g_errno = 24;
}

static void bios_lseek(CpuContext *c)
{
    int s = fd_slot(A0);
    if (s < 0) { V0 = 0xFFFFFFFFu; g_errno = 9; return; }
    int pos = A2 == 0 ? (int)A1 : A2 == 1 ? g_files[s].pos + (int)A1 : A2 == 2 ? g_files[s].size + (int)A1 : g_files[s].pos;
    if (pos < 0) pos = 0;
    if (pos > g_files[s].size) pos = g_files[s].size;
    g_files[s].pos = pos;
    V0 = (uint32_t)pos; g_errno = 0;
}

/* The file's byte at `pos` in the image, or -1 past its blocks. */
static int file_offset(int s, int pos)
{
    int bi = pos / BLK;
    return bi < g_files[s].count ? g_files[s].blocks[bi] * BLK + pos % BLK : -1;
}

static void bios_read(CpuContext *c)
{
    int s = fd_slot(A0);
    if (s < 0) { V0 = 0xFFFFFFFFu; g_errno = 9; return; }
    use_card(g_files[s].card);
    int left = g_files[s].size - g_files[s].pos, n = (int)A2 < left ? (int)A2 : left;
    for (int i = 0; i < n; i++) {
        int o = file_offset(s, g_files[s].pos + i);
        mem_w8(A1 + (uint32_t)i, o >= 0 ? g_card[o] : 0);
    }
    g_files[s].pos += n;
    bios_deliver_event(0xF4000001u, 0x0004u);
    V0 = (uint32_t)n; g_errno = 0;
}

static void bios_write(CpuContext *c)
{
    int s = fd_slot(A0);
    if (s < 0) { V0 = A0 <= 2u ? A2 : 0xFFFFFFFFu; g_errno = A0 <= 2u ? 0 : 9; return; }   /* 0..2: console */
    use_card(g_files[s].card);
    int left = g_files[s].size - g_files[s].pos, n = (int)A2 < left ? (int)A2 : left;
    int lo = CARD_SIZE, hi = 0;
    for (int i = 0; i < n; i++) {
        int o = file_offset(s, g_files[s].pos + i);
        if (o < 0) continue;
        g_card[o] = (uint8_t)mem_r8(A1 + (uint32_t)i);
        if (o < lo) lo = o;
        if (o + 1 > hi) hi = o + 1;
    }
    if (hi > lo) save_range(lo, hi - lo);
    count_saves();   /* a file's blocks need not be contiguous: the span covers them */
    g_files[s].pos += n;
    bios_deliver_event(0xF4000001u, 0x0004u);
    V0 = (uint32_t)n; g_errno = 0;
}

static void bios_close(CpuContext *c)
{
    int s = fd_slot(A0);
    if (s >= 0) g_files[s].used = 0;
    V0 = A0; g_errno = 0;
}

/* ---- directory, format, delete (BiosA FirstFile/NextFile/CardDelete/CardFormat) ---- */
static char g_find_pattern[24];
static int g_find_next;

static uint32_t next_entry(uint32_t dir)
{
    char n[21];
    for (; g_find_next <= DIRS; g_find_next++) {
        int b = g_find_next;
        if (g_card[b * FR] != 0x51) continue;
        name_of(b, n);
        if (!glob_match(g_find_pattern, n)) continue;
        g_find_next++;
        for (int i = 0; i < 20; i++) mem_w8(dir + (uint32_t)i, i < (int)strlen(n) ? (uint8_t)n[i] : 0);
        mem_w32(dir + 0x14u, 0x50u);
        mem_w32(dir + 0x18u, (uint32_t)file_size(b));
        mem_w32(dir + 0x1Cu, 0); mem_w32(dir + 0x20u, 0); mem_w32(dir + 0x24u, 0);
        return dir;
    }
    return 0;
}

static uint32_t first_file(uint32_t wild, uint32_t dir)
{
    char path[64];
    guest_string(wild, path, sizeof path);
    const char *name = card_path(path);
    if (!name) return 0;
    strncpy(g_find_pattern, name, sizeof g_find_pattern - 1);
    g_find_pattern[sizeof g_find_pattern - 1] = 0;
    g_find_next = 1;
    return next_entry(dir);
}

/* ---- frame access (BiosB.CardRead / CardWrite) -------------------------------------- */
static void frame_read(CpuContext *c)
{
    if (g_ready && A2) {
        use_card((A0 & 0x10u) ? 1 : 0);
        int frame = (int)(A1 & 0x3FFu);
        if (frame < CARD_SIZE / FR)
            for (uint32_t i = 0; i < FR; i++) mem_w8(A2 + i, g_card[frame * FR + (int)i]);
    }
    complete(A0);
    V0 = 1;
}

static void frame_write(CpuContext *c)
{
    if (g_ready && A2) {
        use_card((A0 & 0x10u) ? 1 : 0);
        int frame = (int)(A1 & 0x3FFu);
        if (frame < CARD_SIZE / FR) {
            for (uint32_t i = 0; i < FR; i++) g_card[frame * FR + (int)i] = (uint8_t)mem_r8(A2 + i);
            save_range(frame * FR, FR);
            count_saves();
        }
    }
    complete(A0);
    V0 = 1;
}

int card_bios_a(CpuContext *c, uint32_t fn)
{
    switch (fn) {
    case 0x00: bios_open(c); return 1;
    case 0x01: bios_lseek(c); return 1;
    case 0x02: bios_read(c); return 1;
    case 0x03: bios_write(c); return 1;
    case 0x04: bios_close(c); return 1;
    case 0xAB: case 0xAC: complete(A0); V0 = 1; return 1;   /* _card_info, _card_load */
    }
    return 0;
}

int card_bios_b(CpuContext *c, uint32_t fn)
{
    char path[64];
    switch (fn) {
    case 0x32: bios_open(c); return 1;
    case 0x33: bios_lseek(c); return 1;
    case 0x34: bios_read(c); return 1;
    case 0x35: bios_write(c); return 1;
    case 0x36: bios_close(c); return 1;
    case 0x41:                                                    /* format */
        guest_string(A0, path, sizeof path);
        if (!card_path(path)) { bios_deliver_event(0xF0000011u, 0x8000u); V0 = 0; return 1; }
        card_format();
        bios_deliver_event(0xF0000011u, 0x0004u);
        V0 = 1;
        return 1;
    case 0x42: V0 = first_file(A0, A1); return 1;
    case 0x43: V0 = next_entry(A0); return 1;
    case 0x45:                                                    /* delete */
        guest_string(A0, path, sizeof path);
        if (!card_path(path)) { V0 = 0; return 1; }
        delete_file(card_path(path));
        V0 = 1;
        return 1;
    case 0x4D: complete(A0); V0 = 1; return 1;                    /* _card_info_subfunc */
    case 0x4E: frame_write(c); return 1;
    case 0x4F: frame_read(c); return 1;
    case 0x54: V0 = g_errno; return 1;
    case 0x58: V0 = g_last_chan; return 1;                         /* _card_chan */
    case 0x59: guest_string(A0, path, sizeof path); V0 = card_path(path) ? 1u : 0u; return 1;
    }
    return 0;
}

/* ---- set-up ------------------------------------------------------------------------- */
/* TitleMeta.xbx / SaveMeta.xbx: UTF-16 text with a byte-order mark. */
static void write_meta(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint8_t bom[2] = { 0xFF, 0xFE };
    fwrite(bom, 1, 2, f);
    for (const char *p = text; *p; p++) { uint8_t w[2] = { (uint8_t)*p, 0 }; fwrite(w, 1, 2, f); }
    fclose(f);
}

static void copy_once(const char *from, const char *to)
{
    FILE *t = fopen(to, "rb");
    if (t) { fclose(t); return; }
    FILE *f = fopen(from, "rb");
    if (!f) return;
    t = fopen(to, "wb");
    if (t) {
        static uint8_t buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, n, t);
        fclose(t);
    }
    fclose(f);
}

/* The save folder on the hard disk, for other per-player files (the chosen suit). */
const char *card_dir(void) { return g_ready ? SAVE_DIR : 0; }

void card_init(void)
{
    if (!nxIsDriveMounted('E') && !nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\")) return;
    CreateDirectoryA("E:\\UDATA", NULL);
    CreateDirectoryA(TITLE_DIR, NULL);
    CreateDirectoryA(SAVE_DIR, NULL);
    write_meta(TITLE_DIR "\\TitleMeta.xbx", "TitleName=" OPENSPIDEY_TITLE_NAME "\r\n");
    write_meta(SAVE_DIR "\\SaveMeta.xbx", "Name=Memory Cards\r\n");
    copy_once("D:\\TitleImage.xbx", TITLE_DIR "\\TitleImage.xbx");
    copy_once("D:\\SaveImage.xbx", SAVE_DIR "\\SaveImage.xbx");
    g_ready = 1; rt_card_ready = 1;
    for (int k = 0; k < 2; k++) {
        use_card(k);
        FILE *f = fopen(g_card_file[k], "rb");
        size_t n = 0;
        if (f) { n = fread(g_card, 1, CARD_SIZE, f); fclose(f); }
        if (n != CARD_SIZE || g_card[0] != 0x4D || g_card[1] != 0x43) card_format();
    }
    use_card(0);
    count_saves();
}
