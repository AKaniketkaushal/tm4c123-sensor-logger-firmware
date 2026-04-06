/*
 * UART.h
 * Header for UART.c
 * UART0 : Debug console — PA0/PA1, 115200 baud, PC communication
 * UART1 : BLE module    — PB0/PB1, 38400  baud, HW-290/AT-09
 * Declares extern handles, and all UART function prototypes.
 */

#ifndef MY_UART_H
#define MY_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#define QUEUE_SEND_TIMEOUT  pdMS_TO_TICKS(50)
/* ============================================================
 * Extern Handles
 * Defined in main.c — shared across all modules that print or receive
 * ============================================================ */

extern SemaphoreHandle_t    t_mutex;            // guards UARTPrint() against interleaved output
extern StreamBufferHandle_t uart_str_buffer;    // UART1 ISR → BLE_RECEIVE_TASK
extern StreamBufferHandle_t uart1_str_buffer;   // UART0 ISR → DEBUG_CONSOLE_TASK
extern const TickType_t     buffer_wait_time;   // shared stream buffer receive timeout

/* ============================================================
 * Function Declarations
 * ============================================================ */

// Mutex-protected print to UART0 — safe to call from any task
void UARTPrint(const char *str);

// UART0 ISR — forwards printable ASCII to uart1_str_buffer (→ DEBUG_CONSOLE_TASK)
void UART0_ISR(void);

// UART1 ISR — forwards printable ASCII to uart_str_buffer (→ BLE_RECEIVE_TASK)
void UART1_ISR(void);

// Configures UART0 + UART1 pins, baud rates, FIFOs, interrupts, NVIC
void uart_init(void);

#endif // UART_H
