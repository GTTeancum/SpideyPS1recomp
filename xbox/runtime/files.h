/*
 * File reads and writes through the kernel's file calls (runtime/files.c).
 *
 * nxdk's C library leaks a kernel mutant with every fopen: fopen creates the stream's
 * mutex (NtCreateMutant) and fclose never destroys it. Files opened again and again --
 * a suit's files each time one is chosen, an actor at each level load, the memory card
 * at each save -- go through these instead; a file read once at start-up may use stdio.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* The file's size in bytes, or -1 when it cannot be opened. */
long rt_file_size(const char *path);
/* Read up to n bytes from offset: the count read, or -1 when the file cannot be opened. */
long rt_file_read(const char *path, uint32_t offset, void *buffer, uint32_t n);
/* Write n bytes at offset (truncate: the file is replaced by them). 1: all written. */
int rt_file_write(const char *path, uint32_t offset, const void *data, uint32_t n, int truncate);
/* A small text file read whole into text (NUL-terminated; longer files are cut at
 * size - 1). The length, or -1 when it cannot be opened. */
long rt_file_text(const char *path, char *text, size_t size);
/* The next line of *cursor into line (without its line break, cut at size - 1); the
 * cursor moves past it. 0 at the end. */
int rt_next_line(const char **cursor, char *line, size_t size);
