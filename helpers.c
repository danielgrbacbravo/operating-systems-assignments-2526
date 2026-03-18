/**
 * helpers.c  -  Provided helper functions for Exercise 5: File Writer
 *
 * These functions are provided; you do not need to implement them.
 */

#include "helpers.h"

#include <string.h>

int make_83_name(const char* filename, char name8[8], char ext3[3]) {
    /* initialise with spaces */
    memset(name8, ' ', 8);
    memset(ext3, ' ', 3);

    /* find the dot separating name from extension */
    const char* dot = strrchr(filename, '.');
    int name_len, ext_len = 0;
    const char* ext_start = NULL;

    if (dot && dot != filename) {
        name_len = (int)(dot - filename);
        ext_start = dot + 1;
        ext_len = (int)strlen(ext_start);
    } else {
        name_len = (int)strlen(filename);
    }

    if (name_len > 8 || ext_len > 3 || name_len == 0) return -1;

    for (int i = 0; i < name_len; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        name8[i] = c;
    }
    for (int i = 0; i < ext_len; i++) {
        char c = ext_start[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        ext3[i] = c;
    }

    return 0;
}

uint16_t encode_fat_date(int year, int month, int day) { return (uint16_t)(((year - 1980) << 9) | (month << 5) | day); }

uint16_t encode_fat_time(int hour, int min, int sec) { return (uint16_t)((hour << 11) | (min << 5) | (sec / 2)); }
