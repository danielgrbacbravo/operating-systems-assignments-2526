/**
 * helpers.h  -  Provided helper functions for Exercise 5: File Writer
 *
 * These functions are provided; you do not need to implement them.
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

int make_83_name(const char* filename, char name8[8], char ext3[3]);

uint16_t encode_fat_date(int year, int month, int day);

uint16_t encode_fat_time(int hour, int min, int sec);

#endif /* HELPERS_H */
