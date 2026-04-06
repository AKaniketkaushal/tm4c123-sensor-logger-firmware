/*
 * WDT.h
 * Header for WDT.c
 * Implements a software watchdog layer on top of TM4C123 hardware WDT.
 *
 * How it works:
 *   Each task calls wdt_update(WDT_BIT_x) to set its bit in wdt_status_bits.
 *   WDT_MANAGER_TASK checks every 100ms — if ALL_BITS_WDT are set,
 *   all tasks are alive and it calls wdt_kick() to clear the hardware timer.
 *   If any bit is missing before the 300ms hardware timeout, the WDT fires
 *   and resets the MCU.
 *
 * Timeout : 300ms hardware reset
 * Check   : 100ms poll in WDT_MANAGER_TASK
 */

#ifndef WDT_H
#define WDT_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include<stdbool.h>

#define WDT_TIMEOUT_MS 300

/* ============================================================
 * Per-Task Watchdog Bit Flags
 * Each task owns one bit — must set it within 300ms or WDT fires
 * ============================================================ */



typedef enum
{
    WDT_BIT_BLE_SEND    = (1 << 0),  // BLE_SEND_TASK
    WDT_BIT_BLE_RECV    = (1 << 1),  // BLE_RECEIVE_TASK
    WDT_BIT_SENSOR      = (1 << 2),  // SENSOR_TASK
    WDT_BIT_DBG_CONSOLE = (1 << 3),  // DEBUG_CONSOLE_TASK
    WDT_BIT_DEBUG       = (1 << 4),  // DEBUG_TASK
} WatchdogBits_t;

// All bits OR'd — WDT_MANAGER_TASK kicks hardware only when this is met
#define ALL_BITS_WDT  (WDT_BIT_BLE_SEND    | \
                       WDT_BIT_BLE_RECV    | \
                       WDT_BIT_SENSOR      | \
                       WDT_BIT_DBG_CONSOLE | \
                       WDT_BIT_DEBUG)

                      
/* ============================================================
 * Shared Status Byte
 * Defined in WDT.c — extern here so WDT_MANAGER_TASK can read it
 * DO NOT write to this directly — use wdt_update() which is critical-section protected
 * ============================================================ */

extern uint8_t wdt_status_bits;

/* ============================================================
 * Extern Task Handle
 * Defined in main.c — may be used for suspend/resume during init
 * ============================================================ */

extern TaskHandle_t wdt_manager_handle;

/* ============================================================
 * Function Declarations
 * ============================================================ */

// Sets the calling task's bit in wdt_status_bits (critical-section protected)
void wdt_update(uint8_t bit);

// Clears the hardware WDT interrupt — called by WDT_MANAGER_TASK when all bits set
void wdt_kick(void);

// FreeRTOS task — polls wdt_status_bits every 100ms, kicks WDT if all tasks alive
void WDT_MANAGER_TASK(void *pvParameters);

#endif // WDT_H
