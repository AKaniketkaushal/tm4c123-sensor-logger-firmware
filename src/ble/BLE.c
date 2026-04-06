/*
 * BLE.c
 * BLE driver and tasks for TM4C123GXL
 * Module  : HW-290 / AT-09 (CC2541) on UART1 at 38400 baud
 * Tasks   : BLE_SEND_TASK    — dequeues and transmits data/commands
 *           BLE_RECEIVE_TASK — receives BLE commands, dispatches to SENSOR_TASK
 */

#include "main.h"

/* ============================================================
 * Globals
 * ============================================================ */

int ble_baud_rate = 38400;   // Tracks current UART1 baud — updated by ble_change_baud()

/* ============================================================
 * Queue Helpers
 * ============================================================ */

// Enqueues a data payload to Ble_responses — picked up by BLE_SEND_TASK
void enqueue_data(char *data)
{
    ble_msg msg;
    strncpy(msg.data, data, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data) - 1] = '\0';
    msg.type = BLE_SEND_DATA;
    xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
}

// Enqueues an AT command to Ble_responses — sent only when BLE is disconnected
void dequeue_cmd(char *cmd)
{
    ble_msg msg;
    strncpy(msg.data, cmd, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data) - 1] = '\0';
    msg.type = BLE_SEND_CMD;
    xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
}

/* ============================================================
 * Low-Level Transmit
 * ============================================================ */

// Sends raw data bytes over UART1 (BLE data channel)
void BLE_SEND_DATA_UART(const char *data)
{
    while (*data)
        UARTCharPut(UART1_BASE, *data++);
}

// Sends an AT command over UART1 with \r\n terminator
void BLE_SEND_AT_CMD(const char *cmd)
{
    while (*cmd)
        UARTCharPut(UART1_BASE, *cmd++);
    UARTCharPut(UART1_BASE, '\r');
    UARTCharPut(UART1_BASE, '\n');
}

/* ============================================================
 * Connection Status
 * ============================================================ */

// PB2 is driven HIGH by the BLE module when a central is connected
status ble_check_connection(void)
{
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2))
        return CONNECTED;
    else
        return NOT_CONNECTED;
}

/* ============================================================
 * Init & Config
 * ============================================================ */

void ble_init(void)
{
    // PB2 = connection status input — sampled by ble_check_connection()
    GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_2);

    UARTPrint("Initializing BLE Module...\r\n");
    SysCtlDelay(SysCtlClockGet() / 3);     // ~1s settle time after power-on

    // Wake module and confirm it is responding to AT commands
    UARTPrint("Resetting BLE Module...\r\n");
    BLE_SEND_AT_CMD("AT");
    SysCtlDelay(SysCtlClockGet() / 20);    // ~50ms response window

    // Lock baud rate to 38400 on the module side
    UARTPrint("Setting BLE Baud Rate to 38400...\r\n");
    BLE_SEND_AT_CMD("AT+BAUD6");
    SysCtlDelay(SysCtlClockGet() / 20);

    // Reconfigure UART1 to match — disable interrupts during reconfigure
    UARTPrint("Configuring BLE Module...\r\n");
    UARTIntDisable(UART1_BASE, UART_INT_RX | UART_INT_RT);
    UARTDisable(UART1_BASE);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 38400,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTEnable(UART1_BASE);
    UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);

    // Verify module is alive and set device name
    BLE_SEND_AT_CMD("AT");
    SysCtlDelay(SysCtlClockGet() / 20);
    BLE_SEND_AT_CMD("AT+NAME=AniketBLE");
    SysCtlDelay(SysCtlClockGet() / 20);

    UARTPrint("BLE Module Initialized.\r\n");
}

void ble_change_baud(int baud)
{
    // Baud rate can only be changed while disconnected — module rejects AT cmds otherwise
    status conn_status = ble_check_connection();
    if (conn_status == CONNECTED)
    {
        UARTPrint("BLE connected cannot change the baud rate.\r\n");
        return;
    }

    // Map numeric baud to AT+BAUDx command string
    char baud_cmd[20] = {0};
    switch (baud)
    {
        case 9600:   strcpy(baud_cmd, "AT+BAUD4"); break;
        case 19200:  strcpy(baud_cmd, "AT+BAUD5"); break;
        case 38400:  strcpy(baud_cmd, "AT+BAUD6"); break;
        case 57600:  strcpy(baud_cmd, "AT+BAUD7"); break;
        case 115200: strcpy(baud_cmd, "AT+BAUD8"); break;
        case 230400: strcpy(baud_cmd, "AT+BAUD9"); break;
        default:
            UARTPrint("Unsupported baud rate\r\n");
            return;
    }

    // Send command to module, then update MCU UART1 to match
    dequeue_cmd(baud_cmd);
    vTaskDelay(pdMS_TO_TICKS(100));        // wait for module to apply new baud
    UARTDisable(UART1_BASE);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), baud,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    ble_baud_rate = baud;
}

/* ============================================================
 * BLE_SEND_TASK
 * Dequeues messages from Ble_responses and transmits over UART1.
 * CMD  type: only sent when BLE is disconnected (AT mode)
 * DATA type: sent directly over BLE data channel
 * ============================================================ */

void BLE_SEND_TASK(void *pvParameters)
{
    while (1)
    {
        ble_msg buffer;
        if (xQueueReceive(Ble_responses, &buffer, buffer_wait_time) == pdPASS)
        {
            if (buffer.type == BLE_SEND_CMD)
            {
                // AT commands must be sent in disconnected (command) mode
                if (ble_check_connection() == CONNECTED)
                    UARTPrint("BLE In connected state disconnect it first\r\n");
                else
                {
                    UARTPrint("BLE In not connected state sending command\r\n");
                    BLE_SEND_AT_CMD(buffer.data);
                }
            }
            else if (buffer.type == BLE_SEND_DATA)
            {
                // Echo to debug UART then forward to BLE central
                UARTPrint("Sending data over BLE: ");
                UARTPrint(buffer.data);
                UARTPrint("\r\n");
                BLE_SEND_DATA_UART(buffer.data);
            }
        }

        wdt_update(WDT_BIT_BLE_SEND);
        // vTaskDelay(pdMS_TO_TICKS(10));   // optional throttle — disabled
    }
}

/* ============================================================
 * BLE_RECEIVE_TASK
 * Accumulates characters from UART1 ISR stream buffer into a
 * command string. On timeout with data present, parses the
 * command and dispatches a sensor mode to Ble_commands queue.
 * ============================================================ */

void BLE_RECEIVE_TASK(void *pvParameters)
{
    char     buffer[100];
    char     temp_buffer[100];
    uint32_t bytes_received = 0;
    int      idx            = 0;

    while (1)
    {
        /* --- Accumulate incoming bytes --- */
        bytes_received = xStreamBufferReceive(uart_str_buffer, temp_buffer,
                                              sizeof(temp_buffer) - 1, buffer_wait_time);
        if (bytes_received > 0)
        {
            for (int i = 0; i < bytes_received; i++)
                buffer[idx++] = temp_buffer[i];
        }
        else
        {
            // Timeout with data in buffer — treat as complete command
            if (idx > 0)
            {
                uint8_t mode = 0;
                buffer[idx] = '\0';
                UARTPrint(" BLE Received : ");
                UARTPrint(buffer);
                UARTPrint("\r\n");
                idx = 0;

                /* --- Command dispatch to SENSOR_TASK --- */
                if (strcmp(buffer, "SENSORA") == 0)
                {
                    mode = SENSORA;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSORB") == 0)
                {
                    mode = SENSORB;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSORC") == 0)
                {
                    mode = SENSORC;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSOR_ALL") == 0)
                {
                    mode = SENSOR_ALL;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                // Continuous modes — SENSOR_TASK re-runs every ~1s without new command
                else if (strcmp(buffer, "SENSORA_CONT") == 0)
                {
                    mode = SENSORA_CONT;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSORB_CONT") == 0)
                {
                    mode = SENSORB_CONT;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSORC_CONT") == 0)
                {
                    mode = SENSORC_CONT;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SENSOR_ALL_CONT") == 0)
                {
                    mode = SENSOR_ALL_CONT;
                    xQueueSend(Ble_commands, &mode, QUEUE_SEND_TIMEOUT);
                }
            }
        }

        wdt_update(WDT_BIT_BLE_RECV);
    }
}
