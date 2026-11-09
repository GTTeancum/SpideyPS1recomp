/* File reads and writes through the kernel's file calls: see files.h. */
#include <string.h>
#include <windows.h>
#include "files.h"

static HANDLE open_read(const char *path)
{
    return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
}

long rt_file_size(const char *path)
{
    HANDLE h = open_read(path);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD size = GetFileSize(h, 0);
    CloseHandle(h);
    return size == INVALID_FILE_SIZE ? -1 : (long)size;
}

long rt_file_read(const char *path, uint32_t offset, void *buffer, uint32_t n)
{
    HANDLE h = open_read(path);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD got = 0;
    if (SetFilePointer(h, (LONG)offset, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER) ReadFile(h, buffer, n, &got, 0);
    CloseHandle(h);
    return (long)got;
}

int rt_file_write(const char *path, uint32_t offset, const void *data, uint32_t n, int truncate)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, 0, truncate ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD put = 0;
    int ok = SetFilePointer(h, (LONG)offset, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
             WriteFile(h, data, n, &put, 0) && put == n;
    CloseHandle(h);
    return ok;
}

long rt_file_text(const char *path, char *text, size_t size)
{
    if (!size) return -1;
    long got = rt_file_read(path, 0, text, (uint32_t)(size - 1));
    text[got > 0 ? got : 0] = 0;
    return got;
}

int rt_next_line(const char **cursor, char *line, size_t size)
{
    const char *p = *cursor;
    if (!p || !*p || !size) return 0;
    size_t n = 0;
    while (*p && *p != '\n' && *p != '\r') {
        if (n < size - 1) line[n++] = *p;
        p++;
    }
    line[n] = 0;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    *cursor = p;
    return 1;
}
