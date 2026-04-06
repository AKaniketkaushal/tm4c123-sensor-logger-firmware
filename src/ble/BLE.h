/*
 * BLE.h
 * Header for BLE.c
 * Module  : HW-290 / AT-09 (CC2541) on UART1 at 38400 baud
 * Declares BLE message types, status enum, queue handles,
 * and all BLE function and task prototypes.
 */

#ifndef BLE_H
#define BLE_H


/* ============================================================
 * Enums
 * ============================================================ */

typedef enum {
    CONNECTED,
    NOT_CONNECTED
} status;

typedef enum {
    BLE_SEND_CMD,
    BLE_SEND_DATA
} ble_msg_type;

/* ============================================================
 * BLE Message Struct
 * ============================================================ */

typedef struct
{
    char         data[110];
    ble_msg_type type;
} ble_msg;

/* ============================================================
 * Extern Variables
 * ============================================================ */

extern int                  ble_baud_rate;
extern QueueHandle_t        Ble_responses;
extern QueueHandle_t        Ble_commands;
extern StreamBufferHandle_t uart_str_buffer;
extern const TickType_t     buffer_wait_time;

/* ============================================================
 * Function Declarations
 * ============================================================ */

status   ble_check_connection(void);
void     ble_init(void);
void     ble_change_baud(int baud);
void     BLE_SEND_DATA_UART(const char *data);
void     BLE_SEND_AT_CMD(const char *cmd);
void     enqueue_data(char *data);
void     dequeue_cmd(char *cmd);
void     BLE_SEND_TASK(void *pvParameters);
void     BLE_RECEIVE_TASK(void *pvParameters);

#endif // BLE_H
