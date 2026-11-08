/*
 * Software PlayStation GPU: GP0 commands drawn into a 1024x512 16-bit VRAM, GP1
 * display control. A port of the PC runtime's reference rasteriser
 * (RecompOne.Runtime.Gpu: Gpu.cs, GpuCommands.cs, GpuRaster.cs) without its HLE,
 * provenance and widescreen paths.
 */
#pragma once
#include <stdint.h>

#define VRAM_W 1024
#define VRAM_H 512

extern uint16_t gpu_vram[VRAM_H][VRAM_W];

void gpu_write_gp0(uint32_t word);
/* A word read from RAM at `address`: a polygon vertex there may carry a vertex tag. */
void gpu_write_gp0_at(uint32_t word, uint32_t address);
void gpu_debug_clear(void);   /* geometry_debug=2: the drawing area magenta */
void gpu_census_frame(void);  /* geometry_debug=3: analyse the last frame's world polygons */
void gpu_write_gp1(uint32_t word);
uint32_t gpu_read_status(void);
uint32_t gpu_read_data(void);

/* The displayed area of VRAM as set by GP1(05h..08h). */
typedef struct GpuDisplay {
    int x, y, width, height, rgb24, enabled;
} GpuDisplay;
void gpu_display(GpuDisplay *out);
