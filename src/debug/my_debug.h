/*
 * DEBUG.h
 * Header for DEBUG.c
 * Declares debug mode enum, task functions, and required extern handles.
 */

#ifndef DEBUG_H
#define DEBUG_H

/* ============================================================
 * Debug Mode Enum
 * ============================================================ */

typedef enum
{
    DEBUG_NONE = 0,
    DEBUG_BLE_CHECK_CONNECTION,
    DEBUG_SCAN_I2C,
    DEBUG_READ_SENSORA,         // BMP180
    DEBUG_READ_SENSORB,         // MPU6050
    DEBUG_READ_SENSORC,         // Magnetometer
    DEBUG_READ_ALL_SENSORS,
    DEBUG_SYSTEM_STATUS,
    DEBUG_BLE_BAUD4,
    DEBUG_BLE_BAUD5,
    DEBUG_BLE_BAUD6,
    DEBUG_BLE_BAUD7,
    DEBUG_BLE_BAUD8,
    DEBUG_BLE_BAUD9,
    CALIBRATE_HMC5883L,
    DISPLAY_EEPROM
} debug_t;

/* ============================================================
 * Extern Queue Handle
 * Defined in main.c, used by DEBUG_CONSOLE_TASK and DEBUG_TASK
 * ============================================================ */

extern QueueHandle_t debug_queue;
extern QueueHandle_t i2c_isr_message;
extern StreamBufferHandle_t uart1_str_buffer;
extern const TickType_t buffer_wait_time;

/* ============================================================
 * Extern Task Handles
 * Defined in main.c, used by print_system_status() stack watermarks
 * ============================================================ */

extern TaskHandle_t ble_send_handle;
extern TaskHandle_t ble_recv_handle;
extern TaskHandle_t debug_console_handle;
extern TaskHandle_t sensor_handle;
extern TaskHandle_t debug_handle;

/* ============================================================
 * Function Declarations
 * ============================================================ */

// FreeRTOS task — receives UART0 commands, dispatches to debug_queue
void DEBUG_CONSOLE_TASK(void *pvParameters);

// FreeRTOS task — executes debug actions from debug_queue
void DEBUG_TASK(void *pvParameters);

// Prints heap, uptime, queue occupancy, stack watermarks over UART0
void print_system_status(void);

typedef struct {
    uint32_t days;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} TimeSplit_t;

// Helper to convert FreeRTOS ticks to human-readable time format

#endif // DEBUG_H
