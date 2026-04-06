/*
 * DEBUG.c
 * Debug console and diagnostics tasks for TM4C123GXL
 * DEBUG_CONSOLE_TASK : receives UART0 commands, dispatches to DEBUG_TASK
 * DEBUG_TASK         : executes debug actions (sensor reads, BLE, system status)
 */

#include "main.h"

/* ============================================================
 * DEBUG_CONSOLE_TASK
 * Receives characters from UART0 ISR via stream buffer,
 * assembles them into commands, dispatches to debug_queue.
 * Also drains i2c_isr_message queue and prints I2C errors.
 * ============================================================ */

bool CALIB_HMC5883L = false;

static void debug_long_op_checkpoint(void)
{
    wdt_update(WDT_BIT_DEBUG);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void DEBUG_CONSOLE_TASK(void *pvParameters)
{
    char buffer[100];
    char temp_buffer[100];
    uint32_t bytes_received = 0;
    int idx = 0;
    while (1)
    {
        // uint8_t c;
        // if (xQueueReceive(i2c_isr_message, &c, 0) == pdTRUE)
        // {
        //     if (c & I2C_EVT_NACK_ADDR)
        //     {
        //         UARTPrint("I2C NACK on Address\r\n");
        //     }
        //     if (c & I2C_EVT_NACK_DATA)
        //     {
        //         UARTPrint("I2C NACK on Data\r\n");
        //     }
        //     if (c & I2C_EVT_CLK_TIMEOUT)
        //     {
        //         UARTPrint("I2C Clock Timeout\r\n");
        //     }
        //     if (c & I2C_ARBITRATION_LOST)
        //     {
        //         UARTPrint("I2C Arbitration Lost\r\n");
        //     }
        // }

        bytes_received = xStreamBufferReceive(uart1_str_buffer, temp_buffer, sizeof(temp_buffer) - 1, buffer_wait_time);
        if (bytes_received > 0)
        {
            for (int i = 0; i < bytes_received; i++)
            {
                buffer[idx++] = temp_buffer[i];
            }
        }
        else
        {
            if (idx > 0)
            {
                buffer[idx] = '\0';
                UARTPrint("Received from UART1 ISR: ");
                idx = 0;
                UARTPrint(buffer);
                if (strcmp(buffer, "BLE_CHECK_CONNECTION") == 0)
                {
                    UARTPrint("Debug: Checking BLE connection status\r\n");
                    debug_t debug_mode = DEBUG_BLE_CHECK_CONNECTION;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "I2C_SCAN") == 0)
                {
                    UARTPrint("Debug: Scanning I2C bus\r\n");
                    debug_t debug_mode = DEBUG_SCAN_I2C;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "READ_SENSORA") == 0)
                {
                    UARTPrint("Debug: Reading Sensor A (BMP180)\r\n");
                    debug_t debug_mode = DEBUG_READ_SENSORA;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "READ_SENSORB") == 0)
                {
                    UARTPrint("Debug: Reading Sensor B (MPU6050)\r\n");
                    debug_t debug_mode = DEBUG_READ_SENSORB;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "READ_SENSORC") == 0)
                {
                    UARTPrint("Debug: Reading Sensor C (HMC5883L)\r\n");
                    debug_t debug_mode = DEBUG_READ_SENSORC;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "READ_ALL_SENSORS") == 0)
                {
                    UARTPrint("Debug: Reading All Sensors\r\n");
                    debug_t debug_mode = DEBUG_READ_ALL_SENSORS;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "SYSTEM_STATUS") == 0)
                {
                    UARTPrint("Debug: System Status\r\n");
                    debug_t debug_mode = DEBUG_SYSTEM_STATUS;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD4") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 9600\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD4;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD5") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 19200\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD5;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD6") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 38400\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD6;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD7") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 57600\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD7;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD8") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 115200\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD8;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "BLE_BAUD9") == 0)
                {
                    UARTPrint("Debug: Changing BLE baud rate to 230400\r\n");
                    debug_t debug_mode = DEBUG_BLE_BAUD9;
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "CALIBRATE_HMC5883L") == 0)
                {
                    debug_t debug_mode = CALIBRATE_HMC5883L;
                    UARTPrint("Debug: Calibrating HMC5883L\r\n");
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                    CALIB_HMC5883L = true;
                }
                else if (strcmp(buffer, "CALIB_STOP") == 0)
                {
                    UARTPrint("Debug: Stopping HMC5883L Calibration\r\n");
                    CALIB_HMC5883L = false;
                }
                else if (strcmp(buffer, "display_eeprom") == 0)
                {
                    debug_t debug_mode = DISPLAY_EEPROM;
                    UARTPrint("Debug: Displaying EEPROM contents\r\n");
                    xQueueSend(debug_queue, &debug_mode, QUEUE_SEND_TIMEOUT);
                }
                else if (strcmp(buffer, "HELP") == 0)
                {
                    UARTPrint("Available Commands:\r\n");
                    UARTPrint("BLE_CHECK_CONNECTION - Check if BLE module is connected\r\n");
                    UARTPrint("I2C_SCAN - Scan I2C bus for devices\r\n");
                    UARTPrint("READ_SENSORA - Read BMP180 sensor data\r\n");
                    UARTPrint("READ_SENSORB - Read MPU6050 sensor data\r\n");
                    UARTPrint("READ_SENSORC - Read HMC5883L sensor data\r\n");
                    UARTPrint("READ_ALL_SENSORS - Read all sensors data\r\n");
                    UARTPrint("SYSTEM_STATUS - Print system diagnostics\r\n");
                    UARTPrint("BLE_BAUD4 - Set BLE baud to 9600\r\n");
                    UARTPrint("BLE_BAUD5 - Set BLE baud to 19200\r\n");
                    UARTPrint("BLE_BAUD6 - Set BLE baud to 38400\r\n");
                    UARTPrint("BLE_BAUD7 - Set BLE baud to 57600\r\n");
                    UARTPrint("BLE_BAUD8 - Set BLE baud to 115200\r\n");
                    UARTPrint("BLE_BAUD9 - Set BLE baud to 230400\r\n");
                    UARTPrint("CALIBRATE_HMC5883L - Start HMC5883L calibration\r\n");
                    UARTPrint("CALIB_STOP - Stop HMC5883L calibration\r\n");
                    UARTPrint("display_eeprom - Display EEPROM contents\r\n");
                }

                else
                {
                    UARTPrint("Unknown command received from UART1 ISR\r\n");
                }
            }
        }
        wdt_update(WDT_BIT_DBG_CONSOLE);
    }
}

/* ============================================================
 * print_system_status
 * Prints heap usage, uptime, queue occupancy, stack watermarks.
 * Uses heap-allocated buffer to avoid large stack frame.
 * ============================================================ */

void print_system_status(void)
{
    /* --- BLE connection state --- */
    status ble_status = ble_check_connection();
    Uart_Printf("System Status:\r\nBLE: %s\r\n",
                ble_status == CONNECTED ? "Connected" : "Not Connected");

    /* --- Heap usage --- */
    size_t free_heap = xPortGetFreeHeapSize();
    size_t minheap = xPortGetMinimumEverFreeHeapSize();
    Uart_Printf("Free Heap: %u bytes\r\nMin Ever Free: %u bytes\r\n",
                (unsigned)free_heap, (unsigned)minheap);

    /* --- Uptime --- */
    TickType_t ticks = xTaskGetTickCount();
    uint32_t uptime_sec = ticks / configTICK_RATE_HZ;
    Uart_Printf("Uptime: %u sec | Ticks: %u\r\n",
                (unsigned)uptime_sec, (unsigned)ticks);

    /* --- Queue occupancy --- */
    UARTPrint("\r\n-- Queue Occupancy --\r\n");
    Uart_Printf("Ble_commands  : %u\r\n", (unsigned)uxQueueMessagesWaiting(Ble_commands));
    Uart_Printf("Ble_responses : %u\r\n", (unsigned)uxQueueMessagesWaiting(Ble_responses));
    Uart_Printf("debug_queue   : %u\r\n", (unsigned)uxQueueMessagesWaiting(debug_queue));
    Uart_Printf("i2c_isr_msg   : %u\r\n", (unsigned)uxQueueMessagesWaiting(i2c_isr_message));

    /* --- Stack watermarks --- */
    UARTPrint("\r\n-- Stack Watermarks (words free) --\r\n");
    Uart_Printf("BLE_SEND    : %u\r\n", (unsigned)uxTaskGetStackHighWaterMark(ble_send_handle));
    Uart_Printf("BLE_RECV    : %u\r\n", (unsigned)uxTaskGetStackHighWaterMark(ble_recv_handle));
    Uart_Printf("DBG_CONSOLE : %u\r\n", (unsigned)uxTaskGetStackHighWaterMark(debug_console_handle));
    Uart_Printf("SENSOR      : %u\r\n", (unsigned)uxTaskGetStackHighWaterMark(sensor_handle));
    Uart_Printf("DEBUG       : %u\r\n", (unsigned)uxTaskGetStackHighWaterMark(debug_handle));

#if (configUSE_TRACE_FACILITY == 1 && configUSE_STATS_FORMATTING_FUNCTIONS == 1)
    {
        static char task_buf[400]; // static — off the stack
        UARTPrint("\r\n-- Task List --\r\n");
        vTaskList(task_buf);
        UARTPrint(task_buf); // vTaskList output is long — bypass Uart_Printf size check
    }
#endif

#if (configGENERATE_RUN_TIME_STATS == 1)
    {
        static char stat_buf[400];
        UARTPrint("\r\n-- CPU Runtime Stats --\r\n");
        UARTPrint("Name            Abs Time    % Time\r\n");
        vTaskGetRunTimeStats(stat_buf);
        UARTPrint(stat_buf);
    }
#endif
}

void calib_hmc5883l()
{
    UARTPrint("Starting HMC5883L calibration...\r\n");
    UARTPrint("Please rotate the sensor slowly in all orientations...\r\n");
    UARTPrint("To stop calibration send command CALIB_STOP\r\n");

    int16_t x_min = 32767, x_max = -32768;
    int16_t y_min = 32767, y_max = -32768;
    int16_t z_min = 32767, z_max = -32768;
    uint8_t ticks = 0;

    while (1)
    {
        if (CALIB_HMC5883L == true)
        {
            int retries = 20;
            uint8_t status = 0;
            do
            {
                status = 0;
                i2c_read_byte_addr(I2C2_BASE, MAG_ADDR, 0x06, &status);

                if (status & 0x01)
                    break;

                vTaskDelay(pdMS_TO_TICKS(5));
            } while (--retries > 0);

            if (retries == 0)
            {
                UARTPrint("WARNING: Mag data not ready after timeout\r\n");
                wdt_update(WDT_BIT_DEBUG);
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            uint8_t data[6];
            i2c_read_buffer_addr(I2C2_BASE, MAG_ADDR, 0x01, data, 6);
            int16_t x = (int16_t)(data[0] << 8 | data[1]);
            int16_t y = (int16_t)(data[2] << 8 | data[3]);
            int16_t z = (int16_t)(data[4] << 8 | data[5]);
            if (x < x_min)
                x_min = x;
            if (x > x_max)
                x_max = x;
            if (y < y_min)
                y_min = y;
            if (y > y_max)
                y_max = y;
            if (z < z_min)
                z_min = z;
            if (z > z_max)
                z_max = z;
            ticks++;
            if (ticks == 100)
            {
                ticks = 0;
                Uart_Printf("Calib Update | X: %d to %d | Y: %d to %d | Z: %d to %d\r\n", x_min, x_max, y_min, y_max, z_min, z_max);
            }
            wdt_update(WDT_BIT_DEBUG);
        }
    }
}
TimeSplit_t split_time(uint32_t total_sec)
{
    TimeSplit_t t;
    t.days = total_sec / 86400;
    t.hours = (total_sec % 86400) / 3600;
    t.minutes = (total_sec % 3600) / 60;
    t.seconds = total_sec % 60;
    return t;
}

void display_timestamp(uint32_t timestamp)
{
    TimeSplit_t t = split_time(timestamp);
    Uart_Printf("Timestamp: %02d:%02d:%02d\r\n", t.hours, t.minutes, t.seconds);
}

void display_tempearture_pressure(float temperature, float pressure)
{
    int32_t temp_d = 0, temp_f = 0, press_d = 0, press_f = 0;
    split_float(temperature, &temp_d, &temp_f, 2);
    split_float(pressure, &press_d, &press_f, 2);
    Uart_Printf("Temperature: %ld.%02ldC | Pressure: %ld.%02ld hPa\r\n", (long)temp_d, (long)temp_f, (long)press_d, (long)press_f);
}

void display_gyro(float accel_x, float accel_y, float accel_z, float gyro_x, float gyro_y, float gyro_z)
{
    int32_t ax_d = 0, ax_f = 0, ay_d = 0, ay_f = 0, az_d = 0, az_f = 0;
    int32_t gx_d = 0, gx_f = 0, gy_d = 0, gy_f = 0, gz_d = 0, gz_f = 0;
    split_float(accel_x, &ax_d, &ax_f, 2);
    split_float(accel_y, &ay_d, &ay_f, 2);
    split_float(accel_z, &az_d, &az_f, 2);
    split_float(gyro_x, &gx_d, &gx_f, 2);
    split_float(gyro_y, &gy_d, &gy_f, 2);
    split_float(gyro_z, &gz_d, &gz_f, 2);
    Uart_Printf("Accel (g): X=%ld.%02ld | Y=%ld.%02ld | Z=%ld.%02ld\r\nGyro (dps): X=%ld.%02ld | Y=%ld.%02ld | Z=%ld.%02ld\r\n",
                (long)ax_d, (long)ax_f, (long)ay_d, (long)ay_f, (long)az_d, (long)az_f,
                (long)gx_d, (long)gx_f, (long)gy_d, (long)gy_f, (long)gz_d, (long)gz_f);
}

void display_magnetometer(float degrees)
{
    char buf[50];
    int32_t deg_d = 0, deg_f = 0;
    split_float(degrees, &deg_d, &deg_f, 2);
    snprintf(buf, sizeof(buf), "Heading: %ld.%02ld°\r\n", (long)deg_d, (long)deg_f);
    UARTPrint(buf);
}

void display_eeprom_record(EepromRecord_t record)
{
    char buf[200];
    display_timestamp(record.timestamp);
    switch (record.type)
    {
    case REC_TYPE_TEMP_PRESSURE:
        UARTPrint("EEPROM Record: Temperature & Pressure\r\n");
        display_tempearture_pressure(record.temperature, record.pressure);
        break;
    case REC_TYPE_GYRO:
        UARTPrint("EEPROM Record: Gyroscope\r\n");
        display_gyro(record.accel_x, record.accel_y, record.accel_z, record.gyro_x, record.gyro_y, record.gyro_z);
        break;
    case REC_TYPE_MAGNETOMETER:
        UARTPrint("EEPROM Record: Magnetometer\r\n");
        display_magnetometer(record.heading_deg);
        break;
    case REC_TYPE_ALL_SENSORS:
        UARTPrint("EEPROM Record: All Sensors\r\n");
        display_timestamp(record.timestamp);
        display_tempearture_pressure(record.temperature, record.pressure);
        display_gyro(record.accel_x, record.accel_y, record.accel_z, record.gyro_x, record.gyro_y, record.gyro_z);
        display_magnetometer(record.heading_deg);
        break;
    case REC_TYPE_TEMP_PRESSURE_CONT:
        UARTPrint("EEPROM Record: Temperature & Pressure (Continuous)\r\n");
        display_tempearture_pressure(record.temperature, record.pressure);
        break;
    case REC_TYPE_GYRO_CONT:
        UARTPrint("EEPROM Record: Gyroscope (Continuous)\r\n");
        display_gyro(record.accel_x, record.accel_y, record.accel_z, record.gyro_x, record.gyro_y, record.gyro_z);
        break;
        break;
    case REC_TYPE_MAGNETOMETER_CONT:
        UARTPrint("EEPROM Record: Magnetometer (Continuous)\r\n");
        display_magnetometer(record.heading_deg);
        break;
    case REC_TYPE_ALL_SENSORS_CONT:
        UARTPrint("EEPROM Record: All Sensors (Continuous)\r\n");
        display_timestamp(record.timestamp);
        display_tempearture_pressure(record.temperature, record.pressure);
        display_gyro(record.accel_x, record.accel_y, record.accel_z, record.gyro_x, record.gyro_y, record.gyro_z);
        display_magnetometer(record.heading_deg);
        break;
    }
}

void display_eeprom_contents(void)
{
    eeprom_metadata_t meta;
    debug_long_op_checkpoint();
    EEPROMRead((uint32_t *)&meta, 0, sizeof(eeprom_metadata_t));
    debug_long_op_checkpoint();
    Uart_Printf("EEPROM Metadata: Start=%u | End=%u | Count=%u | Magic=0x%04X\r\n",
                meta.start, meta.end, meta.count, meta.magic);
    uint8_t idx = meta.start;

    for (uint8_t i = 0; i < meta.count; i++)
    {
        EepromRecord_t record;
        debug_long_op_checkpoint();
        EEPROMRead((uint32_t *)&record,
                   sizeof(eeprom_metadata_t) + (idx * sizeof(EepromRecord_t)),
                   sizeof(EepromRecord_t));
        debug_long_op_checkpoint();
        display_eeprom_record(record);
        idx = (idx + 1) % EEPROM_QUEUE_LENGTH;
        debug_long_op_checkpoint();
    }
}

/* ============================================================
 * DEBUG_TASK
 * Dequeues debug commands from debug_queue and executes them.
 * Sensor read commands only run when BLE is NOT connected
 * (avoids BLE queue contention during debug sessions).
 * ============================================================ */

void DEBUG_TASK(void *pvParameters)
{
    uint8_t debug_msg;
    while (1)
    {
        if (xQueueReceive(debug_queue, &debug_msg, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            status ble_status = ble_check_connection();
            char buf[40];
            snprintf(buf, sizeof(buf), "DEBUG_TASK received message: 0x%02X\r\n", debug_msg);
            UARTPrint(buf);
            switch (debug_msg)
            {

            case DEBUG_BLE_CHECK_CONNECTION:
                UARTPrint("Debug: Checking BLE connection\r\n");
                if (ble_status == CONNECTED)
                {
                    UARTPrint("BLE is connected\r\n");
                }
                else
                {
                    UARTPrint("BLE is not connected\r\n");
                }
                break;

            case DEBUG_SCAN_I2C:
                UARTPrint("Debug: Scanning I2C bus\r\n");
                i2c_scan();
                break;

            case DEBUG_READ_SENSORA:
                if (ble_status == NOT_CONNECTED)
                {
                    UARTPrint("Debug: Reading Sensor A (BMP180)\r\n");
                    send_temp_pressure_ble();
                }
                break;

            case DEBUG_READ_SENSORB:
                if (ble_status == NOT_CONNECTED)
                {
                    UARTPrint("Debug: Reading Sensor B (MPU6050)\r\n");
                    send_gyro_data_ble();
                }
                break;
            case DEBUG_READ_SENSORC:
                if (ble_status == NOT_CONNECTED)
                {
                    UARTPrint("Debug: Reading Sensor C (Magnetometer)\r\n");
                    send_magnetometer_data_ble();
                }
                break;
            case DEBUG_READ_ALL_SENSORS:
                if (ble_status == NOT_CONNECTED)
                {
                    UARTPrint("Debug: Reading all sensors\r\n");
                    send_temp_pressure_ble();
                    send_gyro_data_ble();
                    send_magnetometer_data_ble();
                }
                break;

            case DEBUG_BLE_BAUD4:
                UARTPrint("Debug: Setting BLE baud rate to 9600\r\n");
                ble_change_baud(9600);
                break;

            case DEBUG_BLE_BAUD5:
                UARTPrint("Debug: Setting BLE baud rate to 19200\r\n");
                ble_change_baud(19200);
                break;

            case DEBUG_BLE_BAUD6:
                UARTPrint("Debug: Setting BLE baud rate to 38400\r\n");
                ble_change_baud(38400);
                break;

            case DEBUG_BLE_BAUD7:
                UARTPrint("Debug: Setting BLE baud rate to 57600\r\n");
                ble_change_baud(57600);
                break;

            case DEBUG_BLE_BAUD8:
                UARTPrint("Debug: Setting BLE baud rate to 115200\r\n");
                ble_change_baud(115200);
                break;

            case DEBUG_BLE_BAUD9:
                UARTPrint("Debug: Setting BLE baud rate to 230400\r\n");
                ble_change_baud(230400);
                break;

            case DEBUG_SYSTEM_STATUS:
                UARTPrint("Debug: System status requested\r\n");
                print_system_status();
                break;

            case CALIBRATE_HMC5883L:
                calib_hmc5883l();
                break;

            case DISPLAY_EEPROM:
                UARTPrint("Debug: Displaying EEPROM contents\r\n");
                display_eeprom_contents();
                break;

            default:
                UARTPrint("Debug: Unknown debug message\r\n");
                break;
            }
        }
        wdt_update(WDT_BIT_DEBUG);
    }
}
