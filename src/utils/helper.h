/*
 * helper.h
 * Header for helper.c
 * Utility functions used across all modules for number formatting.
 * Provides integer-to-string and float-splitting without using
 * sprintf("%f") or printf — safe for bare-metal embedded targets.
 */

#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "semphr.h"

/* ============================================================
 * Function Declarations
 * ============================================================ */

#define LOG_BUF_SMALL   128
#define LOG_BUF_MEDIUM  192
#define LOG_BUF_LARGE   300



void Uart_Printf(const char *format, ...);


/*
 * my_itoa — integer to ASCII string, multi-base
 *
 * Supported bases : 2 (binary), 8 (octal), 10 (decimal), 16 (hex)
 * Negative numbers: handled for base 10 only
 * Output          : null-terminated string written into str
 *
 * Example:
 *   char buf[12];
 *   my_itoa(-42,  buf, 10);  → "-42"
 *   my_itoa(255,  buf, 16);  → "FF"
 *   my_itoa(9,    buf, 2);   → "1001"
 */
void my_itoa(int32_t num, char *str, int base);

/*
 * split_float — splits float into integer and scaled decimal parts
 *
 * Used in place of sprintf("%f") to format floats for UARTPrint
 * and BLE payloads without pulling in heavy printf machinery.
 *
 * Parameters:
 *   value         : float to split
 *   int_part      : receives integer portion (truncated toward zero)
 *   decimal_part  : receives fractional portion × 10^decimal_places
 *   decimal_places: number of decimal digits to preserve
 *
 * Example:
 *   int32_t i, d;
 *   split_float(3.14f, &i, &d, 2);   → i=3,  d=14
 *   split_float(-1.05f, &i, &d, 2);  → i=-1, d=5
 *
 * Caller is responsible for prepending "0" when d < 10 and
 * decimal_places == 2, to correctly print "3.04" not "3.4"
 */
void split_float(float value, int32_t *int_part, int32_t *decimal_part, int decimal_places);

#endif // HELPER_H
