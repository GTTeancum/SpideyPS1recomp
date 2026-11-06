/*
 * The PSY-Q CD library (libcd), a port of the PC runtime's high-level LibCd without
 * XA audio streaming. Sectors come from the player's extracted files on the Xbox
 * disc, placed by the sector map tools/make_disc_map.py writes from the install's
 * recompone-disc.json: "lba size raw2336 path" per file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"

/* ---- disc --------------------------------------------------------------------------- */
typedef struct { int lba, raw; uint32_t size; char path[64]; } DiscFile;

static DiscFile *g_files;
static int g_file_count;
static char g_root[16];
static int g_data_sectors = 0x7FFFFFFF;

int cd_init(const char *disc_root)
{
    char path[260], line[160];
    snprintf(g_root, sizeof g_root, "%s", disc_root);
    snprintf(path, sizeof path, "%s\\disc.map", disc_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int capacity = 0;
    while (fgets(line, sizeof line, f)) {
        int lba, raw;
        unsigned long size;
        char name[96];
        if (strncmp(line, "data_sectors ", 13) == 0) { g_data_sectors = atoi(line + 13); continue; }
        if (sscanf(line, "%d %lu %d %95s", &lba, &size, &raw, name) != 4) continue;
        if (g_file_count == capacity) {
            capacity = capacity ? capacity * 2 : 256;
            g_files = realloc(g_files, (size_t)capacity * sizeof *g_files);
        }
        DiscFile *d = &g_files[g_file_count++];
        d->lba = lba; d->raw = raw; d->size = (uint32_t)size;
        snprintf(d->path, sizeof d->path, "%s", name);
    }
    fclose(f);
    rt_log("disc map: %d files", g_file_count);
    return g_file_count > 0;
}

static uint32_t sector_bytes(const DiscFile *d) { return d->raw ? 2336u : 2048u; }

static const DiscFile *file_at(int lba)
{
    for (int i = 0; i < g_file_count; i++) {
        const DiscFile *d = &g_files[i];
        uint32_t sectors = (d->size + sector_bytes(d) - 1u) / sector_bytes(d);
        if (lba >= d->lba && (uint32_t)(lba - d->lba) < sectors) return d;
    }
    return 0;
}

/* Reads go through the kernel's file calls in 64 KB blocks, a few kept (a load
 * alternates between the archive's directory and the file it finds). The C library's
 * 1 KB buffer, refilled after every per-sector seek, made a mid-level load hundreds of
 * small disk reads (a third of a second's stall in level 3). Files stay open. */
#define BLOCK (64 * 1024)
#define BLOCKS 4
#define HANDLES 8
static struct { const DiscFile *file; HANDLE handle; } g_handles[HANDLES];
static int g_handle_next;
static struct { const DiscFile *file; long at, len; uint32_t used; uint8_t data[BLOCK]; } g_blocks[BLOCKS];
static uint32_t g_block_clock;
volatile uint32_t rt_cd_blocks, rt_cd_us;   /* blocks read, time in the kernel's reads */

static HANDLE open_file(const DiscFile *d)
{
    for (int i = 0; i < HANDLES; i++) if (g_handles[i].file == d) return g_handles[i].handle;
    char path[260];
    snprintf(path, sizeof path, "%s\\%s", g_root, d->path);
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (h == INVALID_HANDLE_VALUE) return h;
    int i = g_handle_next++ % HANDLES;
    if (g_handles[i].file) {
        CloseHandle(g_handles[i].handle);
        for (int b = 0; b < BLOCKS; b++) if (g_blocks[b].file == g_handles[i].file) g_blocks[b].file = 0;
    }
    g_handles[i].file = d; g_handles[i].handle = h;
    return h;
}

/* size bytes at offset of file d; short at its end. */
static void read_bytes(const DiscFile *d, HANDLE f, long offset, uint8_t *out, int size)
{
    while (size > 0) {
        int b = -1;
        for (int i = 0; i < BLOCKS; i++)
            if (g_blocks[i].file == d && offset >= g_blocks[i].at && offset < g_blocks[i].at + g_blocks[i].len) { b = i; break; }
        if (b < 0) {
            b = 0;   /* the least recently used */
            for (int i = 1; i < BLOCKS; i++) if (g_blocks[i].used < g_blocks[b].used) b = i;
            DWORD got = 0;
            long at = offset - offset % BLOCK;
            g_blocks[b].file = 0;
            LARGE_INTEGER t0, t1, hz;
            QueryPerformanceCounter(&t0);
            BOOL ok = SetFilePointer(f, at, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER && ReadFile(f, g_blocks[b].data, BLOCK, &got, 0);
            QueryPerformanceCounter(&t1);
            QueryPerformanceFrequency(&hz);
            rt_cd_blocks++;
            rt_cd_us += (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / hz.QuadPart);
            if (!ok || offset >= at + (long)got) return;   /* unreadable, or past the end of the file */
            g_blocks[b].file = d; g_blocks[b].at = at; g_blocks[b].len = (long)got;
        }
        g_blocks[b].used = ++g_block_clock;
        int n = (int)(g_blocks[b].at + g_blocks[b].len - offset);
        if (n > size) n = size;
        memcpy(out, g_blocks[b].data + (offset - g_blocks[b].at), (size_t)n);
        out += n; offset += n; size -= n;
    }
}

/* Data of one sector: 2048 user bytes, or the 2336 bytes after the sync and header
 * for raw (XA/STR) files. Missing sectors read as zeros. */
static void read_sector(int lba, int size, uint8_t *out)
{
    memset(out, 0, (size_t)size);
    const DiscFile *d = file_at(lba);
    if (!d) { rt_missing("CD", "sector outside the files"); return; }
    HANDLE f = open_file(d);
    if (f == INVALID_HANDLE_VALUE) return;
    long offset = (long)(lba - d->lba) * (long)sector_bytes(d);
    if (d->raw && size == 2048) offset += 8;      /* skip the XA subheader */
    if (!d->raw && size > 2048) { out += size - 2048; size = 2048; }
    read_bytes(d, f, offset, out, size);
}

/* ---- libcd state ---------------------------------------------------------------------- */
enum { Nop = 0x01, Setloc = 0x02, Play = 0x03, ReadN = 0x06, Stop = 0x08, Pause = 0x09, Init = 0x0A,
       Mute = 0x0B, Demute = 0x0C, Setfilter = 0x0D, Setmode = 0x0E, Getparam = 0x0F, GetlocL = 0x10,
       GetlocP = 0x11, GetTN = 0x13, GetTD = 0x14, SeekL = 0x15, SeekP = 0x16, ReadS = 0x1B };
enum { DataReady = 1, Complete = 2, DiskError = 5 };
enum { StatMotor = 0x02, StatRead = 0x20, StatPlay = 0x80 };

static uint8_t g_status = StatMotor, g_mode, g_com, g_pos[4], g_result[8];
static int g_last_intr = Complete, g_read_active, g_pumping;
static uint32_t g_cb_sync, g_cb_ready, g_cb_data;
static uint8_t g_filter_file, g_filter_channel;

static int bcd(uint8_t b) { return (b >> 4) * 10 + (b & 0xF); }
static uint8_t to_bcd(int n) { return (uint8_t)(((n / 10) << 4) + (n % 10)); }
static int pos_to_int(const uint8_t *p) { return (bcd(p[0]) * 60 + bcd(p[1])) * 75 + bcd(p[2]) - 150; }
static void int_to_pos(int i, uint8_t *p)
{
    i += 150;
    p[2] = to_bcd(i % 75); p[1] = to_bcd(i / 75 % 60); p[0] = to_bcd(i / 75 / 60);
}
static int current_lba(void) { return pos_to_int(g_pos); }
static void advance(int n) { int_to_pos(current_lba() + n, g_pos); }
static int audio_region(int lba) { return lba >= g_data_sectors; }
static int sector_size(uint8_t mode) { return (mode & 0x20) ? 2340 : (mode & 0x10) ? 2328 : 2048; }

static void write_result(uint32_t a) { for (int i = 0; i < 8; i++) mem_w8(a + (uint32_t)i, g_result[i]); }

static void set_error(uint8_t err, uint8_t extra)
{
    g_last_intr = DiskError;
    g_result[0] = (uint8_t)(g_status | extra);
    g_result[1] = err;
    memset(g_result + 2, 0, 6);
}

static void reset_state(void)
{
    g_status = StatMotor; g_mode = 0; g_com = 0; g_last_intr = Complete;
    g_cb_sync = g_cb_ready = g_cb_data = 0;
    g_read_active = 0; g_filter_file = g_filter_channel = 0;
    memset(g_pos, 0, sizeof g_pos); memset(g_result, 0, sizeof g_result);
}

static void call_guest(uint32_t fn, uint32_t a0)
{
    CpuContext *c = rt_cpu();
    c->r[4] = a0; c->r[5] = 0;
    recomp_dispatch(c, fn);
}

static void pump_ready(int max_sectors)
{
    if (g_pumping || !g_read_active || !g_cb_ready || g_cb_data || !rt_cpu()) return;
    g_pumping = 1;
    CpuContext saved = *rt_cpu();
    for (int i = 0; i < max_sectors && g_read_active && g_cb_ready; i++) {
        g_last_intr = DataReady;
        call_guest(g_cb_ready, DataReady);
        advance(1);
    }
    *rt_cpu() = saved;
    g_pumping = 0;
}

void cd_tick(void)
{
    if (g_read_active && (g_mode & 0x40)) return;     /* XA audio: not played yet */
    if (!g_read_active || (!g_cb_data && !g_cb_ready) || !rt_cpu()) return;
    if (!g_cb_data) { pump_ready(400000); return; }
    CpuContext saved = *rt_cpu();
    while (g_cb_data) {
        g_last_intr = DataReady;
        if (g_cb_ready) call_guest(g_cb_ready, DataReady);
        advance(1);
        if (g_cb_data) call_guest(g_cb_data, DataReady);
    }
    *rt_cpu() = saved;
}

static int exec_command(uint8_t com, uint32_t param, uint32_t result)
{
    g_com = com;
    g_last_intr = Complete;
    switch (com) {
    case Setloc: if (param) for (int i = 0; i < 4; i++) g_pos[i] = (uint8_t)mem_r8(param + (uint32_t)i); break;
    case Setmode: if (param) g_mode = (uint8_t)mem_r8(param); break;
    case Setfilter: if (param) { g_filter_file = (uint8_t)mem_r8(param); g_filter_channel = (uint8_t)mem_r8(param + 1); } break;
    case ReadN: case ReadS:
        if (audio_region(current_lba()) && !(g_mode & 0x01)) {
            g_read_active = 0;
            set_error(0x40, 0x01);
            if (result) write_result(result);
            return DiskError;
        }
        g_read_active = 1;
        g_status = StatMotor | StatRead;
        break;
    case Play: g_read_active = 0; g_status = StatMotor | StatPlay; break;
    case Getparam:
        g_result[0] = g_status; g_result[1] = g_mode; g_result[2] = 0;
        g_result[3] = g_filter_file; g_result[4] = g_filter_channel; g_result[5] = g_result[6] = g_result[7] = 0;
        if (result) write_result(result);
        return 0;
    case GetlocL:
        g_result[0] = g_pos[0]; g_result[1] = g_pos[1]; g_result[2] = g_pos[2]; g_result[3] = g_mode;
        g_result[4] = g_filter_file; g_result[5] = g_filter_channel; g_result[6] = g_result[7] = 0;
        if (result) write_result(result);
        return 0;
    case GetlocP: {
        uint8_t rel[4];
        int_to_pos(current_lba(), rel);
        g_result[0] = to_bcd(1); g_result[1] = 0x01;
        g_result[2] = rel[0]; g_result[3] = rel[1]; g_result[4] = rel[2];
        g_result[5] = g_pos[0]; g_result[6] = g_pos[1]; g_result[7] = g_pos[2];
        if (result) write_result(result);
        return 0;
    }
    case GetTN:
        g_result[0] = g_status; g_result[1] = to_bcd(1); g_result[2] = to_bcd(1);
        memset(g_result + 3, 0, 5);
        if (result) write_result(result);
        return 0;
    case GetTD: {
        int lba = g_data_sectors == 0x7FFFFFFF ? 0 : g_data_sectors;
        g_result[0] = g_status; g_result[1] = to_bcd(lba / 75 / 60); g_result[2] = to_bcd(lba / 75 % 60);
        memset(g_result + 3, 0, 5);
        if (result) write_result(result);
        return 0;
    }
    case Pause: case Stop: case Init: g_read_active = 0; g_status = StatMotor; break;
    case SeekL:
        if (audio_region(current_lba())) { set_error(0x04, 0x04); if (result) write_result(result); return 0; }
        /* fall through */
    case SeekP: g_read_active = 0; g_status = StatMotor; break;
    default: break;
    }
    g_result[0] = g_status;
    memset(g_result + 1, 0, 7);
    if (result) write_result(result);
    return 0;
}

static int command_wait(uint8_t com, uint32_t param, uint32_t result)
{
    if (param && (com == Play || com == ReadN || com == SeekL || com == SeekP || com == ReadS))
        exec_command(Setloc, param, 0);
    return exec_command(com, param, result);
}

/* ---- the library's entry points (configuration replacements) ------------------------ */
#define A0 c->r[4]
#define A1 c->r[5]
#define A2 c->r[6]
#define V0 c->r[2]

void hook_LibCd_CdInit(CpuContext *c) { reset_state(); g_result[0] = g_status; V0 = 1; }
void hook_LibCd_CdReset(CpuContext *c) { reset_state(); g_result[0] = g_status; V0 = 1; }
void hook_LibCd_CdControl(CpuContext *c) { V0 = command_wait((uint8_t)A0, A1, A2) == 0; }
void hook_LibCd_CdControlF(CpuContext *c) { V0 = command_wait((uint8_t)A0, A1, 0) == 0; }
void hook_LibCd_CdControlB(CpuContext *c)
{
    if (command_wait((uint8_t)A0, A1, A2) != 0) { V0 = 0; return; }
    uint32_t result = A2;
    pump_ready(1);
    if (result) write_result(result);
    V0 = g_last_intr == Complete;
}
void hook_LibCd_CdSync(CpuContext *c) { uint32_t r = A1; pump_ready(1); if (r) write_result(r); V0 = (uint32_t)g_last_intr; }
void hook_LibCd_CdReady(CpuContext *c) { uint32_t r = A1; pump_ready(1); if (r) write_result(r); V0 = (uint32_t)g_last_intr; }

void hook_LibCd_CdRead(CpuContext *c)
{
    rt_note("CdRead");
    static uint8_t sector[2340];
    int sectors = (int)A0;
    uint32_t buf = A1;
    g_mode = (uint8_t)A2;
    int lba = current_lba();
    if (audio_region(lba) && !(g_mode & 0x01)) { set_error(0x40, 0x01); V0 = 1; return; }
    int size = sector_size(g_mode);
    for (int i = 0; i < sectors; i++) {
        read_sector(lba + i, size, sector);
        for (int j = 0; j < size; j++) mem_w8(buf + (uint32_t)(i * size + j), sector[j]);
    }
    g_last_intr = Complete;
    V0 = 1;
}

void hook_LibCd_CdReadSync(CpuContext *c) { if (A1) write_result(A1); V0 = g_last_intr == DiskError ? 0xFFFFFFFFu : 0u; }

void hook_LibCd_CdGetSector(CpuContext *c)
{
    static uint8_t sector[2048];
    read_sector(current_lba(), 2048, sector);
    uint32_t bytes = A1 * 4u < 2048u ? A1 * 4u : 2048u;
    for (uint32_t j = 0; j < bytes; j++) mem_w8(A0 + j, sector[j]);
    V0 = 1;
}

void hook_LibCd_CdDataSync(CpuContext *c) { V0 = 0; }

void hook_LibCd_CdSearchFile(CpuContext *c)
{
    char name[96];
    uint32_t fp = A0;
    int n = 0;
    for (uint32_t a = A1; n < 95; a++) { char ch = (char)mem_r8(a); if (!ch) break; name[n++] = ch; }
    name[n] = 0;
    char *s = name;
    while (*s == '\\' || *s == '/') s++;
    char *semi = strchr(s, ';');
    if (semi) *semi = 0;
    for (char *p = s; *p; p++) if (*p == '\\') *p = '/';
    const DiscFile *found = 0;
    for (int i = 0; i < g_file_count && !found; i++) if (rt_strcasecmp(g_files[i].path, s) == 0) found = &g_files[i];
    if (!found) { rt_log("CdSearchFile: no %s", s); V0 = 0; return; }
    uint8_t pos[4];
    int_to_pos(found->lba, pos);
    mem_w8(fp + 0, pos[0]); mem_w8(fp + 1, pos[1]); mem_w8(fp + 2, pos[2]); mem_w8(fp + 3, 0);
    mem_w32(fp + 4, found->size);
    const char *base = strrchr(found->path, '/');
    base = base ? base + 1 : found->path;
    for (uint32_t i = 0; i < 16; i++) mem_w8(fp + 8 + i, i < strlen(base) ? (uint8_t)base[i] : 0);
    V0 = fp;
}

void hook_LibCd_CdSyncCallback(CpuContext *c) { V0 = g_cb_sync; g_cb_sync = A0; }
void hook_LibCd_CdReadyCallback(CpuContext *c) { V0 = g_cb_ready; g_cb_ready = A0; }
void hook_LibCd_CdReadCallback(CpuContext *c) { V0 = g_cb_data; g_cb_data = A0; }
void hook_LibCd_CdDataCallback(CpuContext *c) { V0 = g_cb_data; g_cb_data = A0; }
void hook_LibCd_CdStatus(CpuContext *c) { pump_ready(1); V0 = g_status; }
void hook_LibCd_CdMode(CpuContext *c) { V0 = g_mode; }
void hook_LibCd_CdLastCom(CpuContext *c) { V0 = g_com; }
void hook_LibCd_CdMix(CpuContext *c) { V0 = 1; }
