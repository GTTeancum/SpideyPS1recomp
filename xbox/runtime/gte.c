/*
 * Geometry transformation engine (COP2): registers only for now. Commands are
 * reported as missing; the PC runtime's Hardware/Gte.cs is the reference to port.
 */
#include "runtime.h"

static uint32_t g_data[32], g_control[32];

void gte_execute(uint32_t instruction) { (void)instruction; rt_missing("GTE", "command"); }
uint32_t gte_read_data(int reg) { return g_data[reg & 31]; }
uint32_t gte_read_control(int reg) { return g_control[reg & 31]; }
void gte_write_data(int reg, uint32_t value) { g_data[reg & 31] = value; }
void gte_write_control(int reg, uint32_t value) { g_control[reg & 31] = value; }
int gte_condition(void) { return 0; }
