/*
 * helper.c
 * Utility functions for TM4C123GXL
 * my_itoa    : integer to ASCII string conversion (multi-base)
 * split_float: splits a float into integer and decimal parts for
 *              formatted printing without using sprintf %f
 */

#include "main.h"
#include <stdarg.h>

SemaphoreHandle_t print_mutex = NULL;

void Uart_Printf(const char *format, ...)
{

    if (print_mutex == NULL)
    {
        print_mutex = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(print_mutex, pdMS_TO_TICKS(40)) == pdTRUE)
    {
        char buffer[LOG_BUF_MEDIUM];
        if (format == NULL)
        {
            UARTPrint("Uart_Printf: NULL format string\r\n");
            xSemaphoreGive(print_mutex);
            return;
        }

        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);

        int size = vsnprintf(NULL, 0, format, args);
        va_end(args);

        if (size < 0 || size >= LOG_BUF_MEDIUM)
        {
            UARTPrint("Uart_Printf: Encoding error in format string\r\n");
            va_end(args_copy);
            xSemaphoreGive(print_mutex);
            return;
        }

        vsnprintf(buffer, size + 1, format, args_copy);
        va_end(args_copy);
        UARTPrint(buffer);
        xSemaphoreGive(print_mutex);
    }
    else
    {
        UARTPrint("Uart_Printf: Failed to acquire print mutex\r\n");
    }
}

/* ============================================================
 * my_itoa
 * Converts int32_t to string in the given base (2/8/10/16).
 * Digits are built in reverse then flipped in-place.
 * Handles negative numbers for base 10 only.
 * ============================================================ */

void my_itoa(int32_t num, char *str, int base)
{
    switch (base)
    {
    /* --- Base 10 (decimal) --- */
    case 10:
    {
        bool is_negative = false;
        uint32_t temp;

        // Cast negative numbers safely — avoids UB on INT32_MIN
        if (num < 0)
        {
            is_negative = true;
            temp = (uint32_t)-(num + 1) + 1;
        }
        else
        {
            temp = (uint32_t)num;
        }

        // Extract digits least-significant first
        int i = 0;
        do
        {
            str[i++] = temp % 10 + '0';
            temp /= 10;
        } while (temp);

        if (is_negative)
            str[i++] = '-';
        str[i] = '\0';

        // Reverse digits to correct order
        int start = 0, end = i - 1;
        while (start < end)
        {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
            start++;
            end--;
        }
    }
    break;

    /* --- Base 16 (hexadecimal) --- */
    case 16:
    {
        uint32_t temp = (uint32_t)num;
        int i = 0;
        do
        {
            uint8_t rem = temp % 16;
            str[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
            temp /= 16;
        } while (temp);
        str[i] = '\0';

        // Reverse digits to correct order
        int start = 0, end = i - 1;
        while (start < end)
        {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
            start++;
            end--;
        }
    }
    break;

    /* --- Base 2 (binary) --- */
    case 2:
    {
        uint32_t temp = (uint32_t)num;
        int i = 0;
        do
        {
            str[i++] = (temp % 2) + '0';
            temp /= 2;
        } while (temp);
        str[i] = '\0';

        // Reverse digits to correct order
        int start = 0, end = i - 1;
        while (start < end)
        {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
            start++;
            end--;
        }
    }
    break;

    /* --- Base 8 (octal) --- */
    case 8:
    {
        uint32_t temp = (uint32_t)num;
        int i = 0;
        do
        {
            str[i++] = (temp % 8) + '0';
            temp /= 8;
        } while (temp);
        str[i] = '\0';

        // Reverse digits to correct order
        int start = 0, end = i - 1;
        while (start < end)
        {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
            start++;
            end--;
        }
    }
    break;

    default:
        UARTPrint("Unsupported base for itoa\r\n");
        str[0] = '\0';
        return;
    }
}

/* ============================================================
 * split_float
 * Splits a float into its integer and fractional parts as
 * int32_t values, scaled by 10^decimal_places.
 *
 * Example: split_float(3.14, &i, &d, 2) → i=3, d=14
 *          split_float(-1.05, &i, &d, 2) → i=-1, d=5
 *
 * Used in place of sprintf("%f") which is unavailable or
 * too heavy on bare-metal targets.
 * ============================================================ */

void split_float(float value, int32_t *int_part, int32_t *decimal_part, int decimal_places)
{
    int multiplier = pow(10, decimal_places); // e.g. 2 places → 100

    int32_t ii = (int32_t)value;  // truncate toward zero
    float dd = value - (float)ii; // fractional remainder

    // Make fractional part positive (handles negative floats)
    if (dd < 0)
        dd = -dd;

    dd = dd * multiplier; // shift decimal into integer range

    *int_part = ii;
    *decimal_part = dd;
}
