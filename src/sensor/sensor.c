/*
 * SENSOR.c
 * Sensor drivers and SENSOR_TASK for TM4C123GXL
 * Sensors: BMP180 (temp/pressure), MPU6050 (accel/gyro), HMC5883L (magnetometer)
 */

#include "main.h"
#include "timers.h"

bmp180_t bmp180;
HMC5883L_data_t hmc5883l;
MPU_6050_data_t mpu6050_data;
static bool is_bmp180_ok = false;
static bool is_mpu6050_ok = false;
static bool is_hmc5883l_ok = false;
static TimerHandle_t send_timer = NULL;
static TimerHandle_t store_timer = NULL;
#define NOTIFY_SEND (1 << 0)
#define NOTIFY_STORE (1 << 1)
extern TaskHandle_t sensor_handle;

void eeprom_init()
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0))
        ;
    EEPROMInit();
    eeprom_metadata_t meta;
    EEPROMRead((uint32_t *)&meta, 0, sizeof(eeprom_metadata_t));
    if (meta.magic != 0xAB)
    {
        meta.magic = 0xAB;
        meta.start = 0;
        meta.end = 0;
        meta.count = 0;
        EEPROMProgram((uint32_t *)&meta, 0, sizeof(eeprom_metadata_t));
    }
}

void get_calibration_data(void)
{
    i2c_status_t status;
    uint8_t calib[22];
    status = i2c_read_buffer_addr(I2C2_BASE, BMP_180_ADDR, 0xAA, calib, 22);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to read BMP180 calibration data\r\n");
        bmp180.is_calibrated = false;
        return;
    }

    for (int i = 0; i < 22; i++)
        bmp180.data[i] = calib[i];
    bmp180.is_calibrated = true;
    is_bmp180_ok = (status == I2C_OK);
}

/* ============================================================
 * BMP180 — Temperature & Pressure Sensor
 * ============================================================ */

void bmp180_init(void)
{
    UARTPrint("Initializing BMP180...\r\n");
    bmp180.is_calibrated = false;
    bmp180.temperature = 0;
    bmp180.pressure = 0;
    for (int i = 0; i < 22; i++)
        bmp180.data[i] = 0;
    get_calibration_data();
}

bool read_bmp180_data(void)
{
    if (!is_bmp180_ok)
        return false;

    i2c_status_t status;
    uint8_t buf[40];

    // Copy stored factory calibration coefficients
    uint8_t calib[22];
    for (int i = 0; i < 22; i++)
        calib[i] = bmp180.data[i];

    // Parse calibration coefficients from datasheet
    int16_t AC1 = (calib[0] << 8) | calib[1];
    int16_t AC2 = (calib[2] << 8) | calib[3];
    int16_t AC3 = (calib[4] << 8) | calib[5];
    uint16_t AC4 = (calib[6] << 8) | calib[7];
    uint16_t AC5 = (calib[8] << 8) | calib[9];
    uint16_t AC6 = (calib[10] << 8) | calib[11];
    int16_t B1 = (calib[12] << 8) | calib[13];
    int16_t B2 = (calib[14] << 8) | calib[15];
    int16_t MB = (calib[16] << 8) | calib[17];
    int16_t MC = (calib[18] << 8) | calib[19];
    int16_t MD = (calib[20] << 8) | calib[21];

    // Read raw temperature
    status = i2c_write_addr(I2C2_BASE, 0x77, 0xF4, 0x2E);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to write to BMP180 control register\r\n");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t temp_raw[2];
    status = i2c_read_buffer_addr(I2C2_BASE, 0x77, 0xF6, temp_raw, 2);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to read BMP180 temperature data\r\n");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    int32_t UT = (temp_raw[0] << 8) | temp_raw[1];

    // Read raw pressure
    status = i2c_write_addr(I2C2_BASE, 0x77, 0xF4, 0x34);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to write to BMP180 control register\r\n");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t press_raw[3];
    status = i2c_read_buffer_addr(I2C2_BASE, 0x77, 0xF6, press_raw, 3);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to read BMP180 pressure data\r\n");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    int32_t UP = ((press_raw[0] << 16) | (press_raw[1] << 8) | press_raw[2]) >> 8;

    // Temperature compensation (BMP180 datasheet section 4.1.3)
    int32_t X1 = ((UT - AC6) * AC5) >> 15;
    int32_t X2 = (MC << 11) / (X1 + MD);
    int32_t B5 = X1 + X2;
    int32_t temperature = (B5 + 8) >> 4;

    // Pressure compensation
    uint8_t oss = 0;
    int32_t B6 = B5 - 4000;
    X1 = (B2 * (B6 * B6 >> 12)) >> 11;
    X2 = (AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = (((((int32_t)AC1 * 4) + X3) << oss) + 2) >> 2;
    X1 = (AC3 * B6) >> 13;
    X2 = (B1 * (B6 * B6 >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = (AC4 * (uint32_t)(X3 + 32768)) >> 15;

    uint64_t B7 = ((uint64_t)UP - B3) * (50000 >> oss);

    int32_t p;
    if (B7 < 0x80000000)
        p = (B7 << 1) / B4;
    else
        p = (B7 / B4) << 1;

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);

    bmp180.pressure = p;
    bmp180.temperature = temperature / 10.0;

    return true;
}

/* ============================================================
 * MPU6050 — Accelerometer & Gyroscope
 * ============================================================ */

void MPU_6050_init(void)
{
    i2c_status_t status;

    // Zero out data struct
    mpu6050_data.accel_x = 0;
    mpu6050_data.accel_y = 0;
    mpu6050_data.accel_z = 0;
    mpu6050_data.gyro_x = 0;
    mpu6050_data.gyro_y = 0;
    mpu6050_data.gyro_z = 0;

    // Reset sensor, then wake with PLL clock source
    status = i2c_write_addr(I2C2_BASE, MPU6050_ADDR, PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, PWR_MGMT_1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t pwr = 0xFF;
    status &= i2c_read_byte_addr(I2C2_BASE, MPU6050_ADDR, PWR_MGMT_1, &pwr);
    char buf[40];
    snprintf(buf, sizeof(buf), "PWR_MGMT_1: 0x%02X (expect 0x01)\r\n", pwr);
    UARTPrint(buf);

    // Disable I2C master mode, enable bypass for direct mag access
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, 0x6A, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, 0x37, 0x02);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t bypass = 0xFF;
    status &= i2c_read_byte_addr(I2C2_BASE, MPU6050_ADDR, 0x37, &bypass);
    vTaskDelay(pdMS_TO_TICKS(100));
    UARTPrint(buf);

    // Configure sample rate, DLPF, gyro/accel range
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, SMPLRT_DIV, 0x07); // 1kHz sample rate
    vTaskDelay(pdMS_TO_TICKS(10));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, CONFIG_REG, 0x03); // DLPF ~44Hz
    vTaskDelay(pdMS_TO_TICKS(10));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, GYRO_CONFIG, 0x00); // ±250°/s
    vTaskDelay(pdMS_TO_TICKS(10));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, ACCEL_CONFIG, 0x00); // ±2g
    vTaskDelay(pdMS_TO_TICKS(10));
    status &= i2c_write_addr(I2C2_BASE, MPU6050_ADDR, INT_ENABLE, 0x01); // Data ready interrupt
    vTaskDelay(pdMS_TO_TICKS(10));

    is_mpu6050_ok = (status == I2C_OK);
}

bool calibrate_mpu6050(void)
{
    // Accumulate 150 samples at rest to compute zero-offset bias
    int32_t acc_x_sum = 0, acc_y_sum = 0, acc_z_sum = 0;
    int32_t gyro_x_sum = 0, gyro_y_sum = 0, gyro_z_sum = 0;
    int good_samples = 0;
    i2c_status_t status;
    uint8_t buf[14]; // ← must be uint8_t to avoid sign extension

    for (int i = 0; i < 150; i++)
    {
        status = i2c_read_buffer_addr(I2C2_BASE, MPU6050_ADDR, ACCEL_XOUT_H, buf, 14);
        if (status != I2C_OK)
        {
            continue;
        }

        acc_x_sum += (int16_t)((buf[0] << 8) | buf[1]);
        acc_y_sum += (int16_t)((buf[2] << 8) | buf[3]);
        acc_z_sum += (int16_t)((buf[4] << 8) | buf[5]);
        gyro_x_sum += (int16_t)((buf[8] << 8) | buf[9]);
        gyro_y_sum += (int16_t)((buf[10] << 8) | buf[11]);
        gyro_z_sum += (int16_t)((buf[12] << 8) | buf[13]);

        vTaskDelay(pdMS_TO_TICKS(2));
        good_samples++;
    }

    if (good_samples < 100)
    {
        UARTPrint("Not enough good samples for MPU6050 calibration\r\n");
        return false;
    }

    // Convert averaged raw counts to physical units and store as bias
    mpu6050_data.accl_calib_x = (acc_x_sum / (float)good_samples) / 16384.0f;
    mpu6050_data.accl_calib_y = (acc_y_sum / (float)good_samples) / 16384.0f;
    mpu6050_data.accl_calib_z = (acc_z_sum / (float)good_samples) / 16384.0f - 1.0f; // remove gravity
    mpu6050_data.gyro_calib_x = (gyro_x_sum / (float)good_samples) / 131.0f;
    mpu6050_data.gyro_calib_y = (gyro_y_sum / (float)good_samples) / 131.0f;
    mpu6050_data.gyro_calib_z = (gyro_z_sum / (float)good_samples) / 131.0f;
    return true;
}

bool read_MPU_6050(void)
{
    uint8_t buffer[14];
    i2c_status_t status = i2c_read_buffer_addr(I2C2_BASE, MPU6050_ADDR, ACCEL_XOUT_H, buffer, 14);
    if (status != I2C_OK)
    {
        UARTPrint("Failed to read MPU6050 data\r\n");
        return false;
    }
    UARTPrint("MPU6050 Raw Data: ");

    // Reconstruct signed 16-bit raw values
    mpu6050_data.accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
    mpu6050_data.accel_y = (int16_t)((buffer[2] << 8) | buffer[3]);
    mpu6050_data.accel_z = (int16_t)((buffer[4] << 8) | buffer[5]);
    mpu6050_data.gyro_x = (int16_t)((buffer[8] << 8) | buffer[9]);
    mpu6050_data.gyro_y = (int16_t)((buffer[10] << 8) | buffer[11]);
    mpu6050_data.gyro_z = (int16_t)((buffer[12] << 8) | buffer[13]);

    // Convert to physical units and apply calibration bias
    mpu6050_data.accel_x_g = (mpu6050_data.accel_x / 16384.0f) - mpu6050_data.accl_calib_x;
    mpu6050_data.accel_y_g = (mpu6050_data.accel_y / 16384.0f) - mpu6050_data.accl_calib_y;
    mpu6050_data.accel_z_g = (mpu6050_data.accel_z / 16384.0f) - mpu6050_data.accl_calib_z;
    mpu6050_data.gyro_x_dps = (mpu6050_data.gyro_x / 131.0f) - mpu6050_data.gyro_calib_x;
    mpu6050_data.gyro_y_dps = (mpu6050_data.gyro_y / 131.0f) - mpu6050_data.gyro_calib_y;
    mpu6050_data.gyro_z_dps = (mpu6050_data.gyro_z / 131.0f) - mpu6050_data.gyro_calib_z;
    return true;
}

/* ============================================================
 * HMC5883L — Magnetometer
 * ============================================================ */

void HMC5883L_init(void)
{
    i2c_status_t status;
    hmc5883l.x_raw = 0;
    hmc5883l.y_raw = 0;
    hmc5883l.z_raw = 0;
    hmc5883l.heading = 0;
    hmc5883l.heading_deg = 0;

    char buf[50];
    uint8_t val = 0;

    // Soft reset — must complete before any config writes
    status = i2c_write_addr(I2C2_BASE, MAG_ADDR, 0x0B, 0x80);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Set SET/RESET period and enable continuous mode
    status &= i2c_write_addr(I2C2_BASE, MAG_ADDR, 0x0B, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));
    status &= i2c_write_addr(I2C2_BASE, MAG_ADDR, 0x0A, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Verify CTRL1 register was written correctly
    status &= i2c_read_byte_addr(I2C2_BASE, MAG_ADDR, 0x0A, &val);
    snprintf(buf, sizeof(buf), "CTRL1 readback: 0x%02X (expect 0x01)\r\n", val);
    UARTPrint(buf);

    if (val != 0x01)
        UARTPrint("WARNING: CTRL1 not 0x01 — trying fallback 0x19\r\n");

    // Wait for first valid sample (200Hz = 5ms/sample, wait 3 cycles)
    vTaskDelay(pdMS_TO_TICKS(50));
    UARTPrint("Mag init complete\r\n");
    is_hmc5883l_ok = (status == I2C_OK);
}

bool read_HMC5883L(void)
{
    // Poll status register until data ready (max 10 retries)
    int retries = 10;
    uint8_t status = 0;
    i2c_status_t i2c_status;
    do
    {
        status = 0;
        i2c_status = i2c_read_byte_addr(I2C2_BASE, MAG_ADDR, 0x06, &status);
        if (i2c_status != I2C_OK)
        {
            UARTPrint("Failed to read HMC5883L status register\r\n");
            return false;
        }
        if (status & 0x01)
            break;
        vTaskDelay(pdMS_TO_TICKS(5));
    } while (--retries > 0);

    if (retries == 0)
    {
        UARTPrint("WARNING: Mag data not ready after timeout\r\n");
        return false;
    }

    // Read 6 bytes: X[1:0], Y[1:0], Z[1:0]
    uint8_t data[6];
    i2c_status = i2c_read_buffer_addr(I2C2_BASE, MAG_ADDR, 0x01, data, 6);
    if (i2c_status != I2C_OK)
    {
        UARTPrint("Failed to read HMC5883L data\r\n");
        return false;
    }

    hmc5883l.x_raw = (int16_t)((data[0] << 8) | data[1]);
    hmc5883l.y_raw = (int16_t)((data[2] << 8) | data[3]);
    hmc5883l.z_raw = (int16_t)((data[4] << 8) | data[5]);

    // Compute heading with Delhi magnetic declination correction
    float heading = atan2f((float)hmc5883l.y_raw, (float)hmc5883l.x_raw);
    heading += 0.0965f; // +5.53° declination = 0.0965 rad
    if (heading < 0)
        heading += 2.0f * M_PI;
    if (heading > 2.0f * M_PI)
        heading -= 2.0f * M_PI;

    hmc5883l.heading = heading;
    hmc5883l.heading_deg = heading * 180.0f / M_PI;
    return true;
}

/* ============================================================
 * BLE Data Senders
 * ============================================================ */

void send_temp_pressure_ble(void)
{
    ble_msg msg;
    memset(&msg, 0, sizeof(msg));
    if (!read_bmp180_data())
    {
        UARTPrint("Failed to read BMP180 data\r\n");
        strcat(msg.data, "SENSORA_DATA | Error reading BMP180");
        UARTPrint(msg.data);
        UARTPrint("\r\n");
        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
    }
    else
    {
        int32_t td = 0, tf = 0;
        split_float(bmp180.temperature, &td, &tf, 2);
        msg.type = BLE_SEND_DATA;

        char ttds[12], ttfs[12], pps[12];
        my_itoa(td, ttds, 10);
        my_itoa(tf, ttfs, 10);
        my_itoa(bmp180.pressure, pps, 10);

        strcat(msg.data, "SENSORA_DATA | Temp: ");
        strcat(msg.data, ttds);
        strcat(msg.data, ".");
        if (tf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, ttfs);
        strcat(msg.data, " | Press: ");
        strcat(msg.data, pps);
        strcat(msg.data, " Pa");

        UARTPrint(msg.data);
        UARTPrint("\r\n");

        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
    }
}

void send_gyro_data_ble(void)
{
    ble_msg msg;
    memset(&msg, 0, sizeof(msg));
    if (!read_MPU_6050())
    {
        UARTPrint("Failed to read MPU6050 data\r\n");
        strcat(msg.data, "SENSORB_DATA | Error reading MPU6050");
        UARTPrint(msg.data);
        UARTPrint("\r\n");
        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
    }
    else
    {
        msg.type = BLE_SEND_DATA;

        int32_t axd = 0, ayd = 0, azd = 0, gxd = 0, gyd = 0, gzd = 0;
        int32_t axf = 0, ayf = 0, azf = 0, gxf = 0, gyf = 0, gzf = 0;

        split_float(mpu6050_data.accel_x_g, &axd, &axf, 2);
        split_float(mpu6050_data.accel_y_g, &ayd, &ayf, 2);
        split_float(mpu6050_data.accel_z_g, &azd, &azf, 2);
        split_float(mpu6050_data.gyro_x_dps, &gxd, &gxf, 2);
        split_float(mpu6050_data.gyro_y_dps, &gyd, &gyf, 2);
        split_float(mpu6050_data.gyro_z_dps, &gzd, &gzf, 2);

        char axds[12], ayds[12], azds[12], gxdss[12], gyds[12], gzds[12];
        char axfs[12], ayfs[12], azfs[12], gxfs[12], gyfs[12], gzfs[12];

        my_itoa(axd, axds, 10);
        my_itoa(axf, axfs, 10);
        my_itoa(ayd, ayds, 10);
        my_itoa(ayf, ayfs, 10);
        my_itoa(azd, azds, 10);
        my_itoa(azf, azfs, 10);
        my_itoa(gxd, gxdss, 10);
        my_itoa(gxf, gxfs, 10);
        my_itoa(gyd, gyds, 10);
        my_itoa(gyf, gyfs, 10);
        my_itoa(gzd, gzds, 10);
        my_itoa(gzf, gzfs, 10);

        strcat(msg.data, "SENSORB_DATA | Accel (g): ");
        strcat(msg.data, "X=");
        strcat(msg.data, axds);
        strcat(msg.data, ".");
        if (axf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, axfs);
        strcat(msg.data, " Y=");
        strcat(msg.data, ayds);
        strcat(msg.data, ".");
        if (ayf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, ayfs);
        strcat(msg.data, " Z=");
        strcat(msg.data, azds);
        strcat(msg.data, ".");
        if (azf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, azfs);

        strcat(msg.data, " | Gyro : X=");
        strcat(msg.data, gxdss);
        strcat(msg.data, ".");
        if (gxf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, gxfs);
        strcat(msg.data, " Y=");
        strcat(msg.data, gyds);
        strcat(msg.data, ".");
        if (gyf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, gyfs);
        strcat(msg.data, " Z=");
        strcat(msg.data, gzds);
        strcat(msg.data, ".");
        if (gzf < 10)
            strcat(msg.data, "0");
        strcat(msg.data, gzfs);

        UARTPrint("Read accelerometer and gyroscope data from MPU6050 sensor\r\n");
        UARTPrint(msg.data);
        UARTPrint("\r\n");

        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
    }
}

void send_magnetometer_data_ble(void)
{
    ble_msg msg;
    memset(&msg, 0, sizeof(msg));
    if (!read_HMC5883L())
    {
        UARTPrint("Failed to read HMC5883L data\r\n");
        strcat(msg.data, "SENSORB_DATA | Error reading HMC5883L");
        UARTPrint(msg.data);
        UARTPrint("\r\n");
        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
        return;
    }

    else
    {
        int32_t hd_d = 0, hd_f = 0;
        split_float(hmc5883l.heading_deg, &hd_d, &hd_f, 2);
        msg.type = BLE_SEND_DATA;

        char hdds[12], hdfs[12];
        my_itoa(hd_d, hdds, 10);
        my_itoa(hd_f, hdfs, 10);

        strcat(msg.data, "SENSORC_DATA | Heading: ");
        strcat(msg.data, hdds);
        strcat(msg.data, ".");
        if (hd_f < 10)
            strcat(msg.data, "0");
        strcat(msg.data, hdfs);

        UARTPrint("Read magnetometer data from HMC5883L sensor\r\n");
        UARTPrint(msg.data);
        UARTPrint("\r\n");

        status ble_status = ble_check_connection();
        if (ble_status == CONNECTED)
            xQueueSend(Ble_responses, &msg, QUEUE_SEND_TIMEOUT);
    }
}

/* ============================================================
 * SENSOR_TASK — FreeRTOS Task
 * ============================================================ */

void fill_bmp180_data(EepromRecord_t *rec, float temperature, float pressure)
{
    rec->temperature = temperature;
    rec->pressure = pressure;
}

void fill_gyro_data(EepromRecord_t *rec, float ax, float ay, float az, float gx, float gy, float gz)
{
    rec->accel_x = ax;
    rec->accel_y = ay;
    rec->accel_z = az;
    rec->gyro_x = gx;
    rec->gyro_y = gy;
    rec->gyro_z = gz;
}

void fill_magnetometer_data(EepromRecord_t *rec, float heading_deg)
{
    rec->heading_deg = heading_deg;
}

void push_to_eeprom(EepromRecord_t *rec)
{
    int start, end, count;
    eeprom_metadata_t meta;
    wdt_update(WDT_BIT_SENSOR);
    EEPROMRead((uint32_t *)&meta, 0, sizeof(eeprom_metadata_t));
    wdt_update(WDT_BIT_SENSOR);
    start = meta.start;
    end = meta.end;
    count = meta.count;
    int temp = end;
    uint32_t write_address = sizeof(eeprom_metadata_t) + temp * sizeof(EepromRecord_t);
    uint32_t start_address = sizeof(eeprom_metadata_t);
    uint32_t write_size = sizeof(EepromRecord_t);

    int status = EEPROMProgram((uint32_t *)rec, write_address, write_size);
    wdt_update(WDT_BIT_SENSOR);
    if (status == 0)
    {
        UARTPrint("Data stored to EEPROM successfully\r\n");
    }
    else
    {
        UARTPrint("Failed to store data to EEPROM\r\n");
    }

    temp = (temp + 1) % EEPROM_QUEUE_LENGTH;

    if (count < EEPROM_QUEUE_LENGTH)
    {
        count++;
        end = temp;
    }
    else
    {
        start = (start + 1) % EEPROM_QUEUE_LENGTH;
        end = temp;
    }

    meta.start = start;
    meta.end = end;
    meta.count = count;
    wdt_update(WDT_BIT_SENSOR);
    EEPROMProgram((uint32_t *)&meta, 0, sizeof(eeprom_metadata_t));
    wdt_update(WDT_BIT_SENSOR);
}

void store_sensor_data(uint8_t mode)
{
    EepromRecord_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.timestamp = xTaskGetTickCount();
    rec.sensor_mode = mode;

    switch (mode)
    {
    case SENSORA:
        rec.type = REC_TYPE_TEMP_PRESSURE;
        fill_bmp180_data(&rec, bmp180.temperature, bmp180.pressure);
        break;
    case SENSORB:
        rec.type = REC_TYPE_GYRO;
        fill_gyro_data(&rec, mpu6050_data.accel_x_g, mpu6050_data.accel_y_g, mpu6050_data.accel_z_g,
                       mpu6050_data.gyro_x_dps, mpu6050_data.gyro_y_dps, mpu6050_data.gyro_z_dps);
        break;
    case SENSORC:
        rec.type = REC_TYPE_MAGNETOMETER;
        fill_magnetometer_data(&rec, hmc5883l.heading_deg);
        break;
    case SENSOR_ALL:
        rec.type = REC_TYPE_ALL_SENSORS;
        fill_bmp180_data(&rec, bmp180.temperature, bmp180.pressure);
        fill_gyro_data(&rec, mpu6050_data.accel_x_g, mpu6050_data.accel_y_g, mpu6050_data.accel_z_g,
                       mpu6050_data.gyro_x_dps, mpu6050_data.gyro_y_dps, mpu6050_data.gyro_z_dps);
        fill_magnetometer_data(&rec, hmc5883l.heading_deg);
        break;
    case SENSORA_CONT:
        rec.type = REC_TYPE_TEMP_PRESSURE_CONT;
        fill_bmp180_data(&rec, bmp180.temperature, bmp180.pressure);
        break;
    case SENSORB_CONT:
        rec.type = REC_TYPE_GYRO_CONT;
        fill_gyro_data(&rec, mpu6050_data.accel_x_g, mpu6050_data.accel_y_g, mpu6050_data.accel_z_g,
                       mpu6050_data.gyro_x_dps, mpu6050_data.gyro_y_dps, mpu6050_data.gyro_z_dps);
        break;
    case SENSORC_CONT:
        rec.type = REC_TYPE_MAGNETOMETER_CONT;
        fill_magnetometer_data(&rec, hmc5883l.heading_deg);
        break;
    case SENSOR_ALL_CONT:
        rec.type = REC_TYPE_ALL_SENSORS_CONT;
        fill_bmp180_data(&rec, bmp180.temperature, bmp180.pressure);
        fill_gyro_data(&rec, mpu6050_data.accel_x_g, mpu6050_data.accel_y_g, mpu6050_data.accel_z_g,
                       mpu6050_data.gyro_x_dps, mpu6050_data.gyro_y_dps, mpu6050_data.gyro_z_dps);
        fill_magnetometer_data(&rec, hmc5883l.heading_deg);
        break;
    default:
        return;
    }
    push_to_eeprom(&rec);
}

void send_timer_cb(TimerHandle_t xTimer)
{
    xTaskNotifyFromISR(sensor_handle, NOTIFY_SEND, eSetBits, NULL);
}

void store_timer_cb(TimerHandle_t xTimer)
{
    xTaskNotifyFromISR(sensor_handle, NOTIFY_STORE, eSetBits, NULL);
}

void init_sensors()
{

    bmp180_init();
    wdt_update(WDT_BIT_SENSOR);
    MPU_6050_init();
    wdt_update(WDT_BIT_SENSOR);
    if (is_mpu6050_ok)
    {
        if (!calibrate_mpu6050())
        {
            UARTPrint("MPU6050 calibration failed\r\n");
        }
        else
        {
            UARTPrint("MPU6050 calibration successful\r\n");
        }
        wdt_update(WDT_BIT_SENSOR);
    }

    HMC5883L_init();
    wdt_update(WDT_BIT_SENSOR);
    UARTPrint("Sensor task initialized sensors\r\n");
}

void process_sensor_mode(uint8_t mode, bool store_enable)
{
    switch (mode)
    {
    case SENSORA:
    case SENSORA_CONT:
        send_temp_pressure_ble();
        if (is_bmp180_ok && store_enable)
            store_sensor_data(mode);
        break;

    case SENSORB:
    case SENSORB_CONT:
        send_gyro_data_ble();
        if (is_mpu6050_ok && store_enable)
            store_sensor_data(mode);
        break;

    case SENSORC:
    case SENSORC_CONT:
        send_magnetometer_data_ble();
        if (is_hmc5883l_ok && store_enable)
            store_sensor_data(mode);
        break;

    case SENSOR_ALL:
    case SENSOR_ALL_CONT:
        send_temp_pressure_ble();
        send_gyro_data_ble();
        send_magnetometer_data_ble();
        if (is_bmp180_ok && is_mpu6050_ok && is_hmc5883l_ok && store_enable)
            store_sensor_data(mode);
        break;
    }
}

void check_sensor_health()
{
    if (!is_bmp180_ok)
    {
        UARTPrint("Reinitializing BMP180...\r\n");
        bmp180_init();
    }
    if (!is_mpu6050_ok)
    {
        UARTPrint("Reinitializing MPU6050...\r\n");
        MPU_6050_init();
        if (is_mpu6050_ok)
        {
            calibrate_mpu6050();
        }
    }
    if (!is_hmc5883l_ok)
    {
        UARTPrint("Reinitializing HMC5883L...\r\n");
        HMC5883L_init();
    }
}

void SENSOR_TASK(void *pvParameters)
{
    UARTPrint("Sensor task started\r\n");
    send_timer = xTimerCreate("send", pdMS_TO_TICKS(1000), pdTRUE, NULL, send_timer_cb);
    store_timer = xTimerCreate("store", pdMS_TO_TICKS(30000), pdTRUE, NULL, store_timer_cb);
    uint8_t reinit_ticks = 0;
    sensor_modes_t mode = SENSOR_MODE_IDLE;

    init_sensors();
    while (1)
    {
        if (xQueueReceive(Ble_commands, &mode, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            switch (mode)
            {
            case SENSORA:
            case SENSORB:
            case SENSORC:
            case SENSOR_ALL:
                xTimerStop(send_timer, 0);
                xTimerStop(store_timer, 0);
                process_sensor_mode(mode, true);
                break;

            case SENSORA_CONT:
            case SENSORB_CONT:
            case SENSORC_CONT:
            case SENSOR_ALL_CONT:
                xTimerStart(send_timer, 0);
                xTimerStart(store_timer, 0);
                break;
            }
        }

        uint32_t flags = 0;
        if (xTaskNotifyWait(0, NOTIFY_SEND | NOTIFY_STORE, &flags, 0) == pdTRUE)
        {
            if (flags & NOTIFY_SEND)
            {
                process_sensor_mode(mode, false);
                wdt_update(WDT_BIT_SENSOR);
            }

            if (flags & NOTIFY_STORE)
            {
                process_sensor_mode(mode, true);
                wdt_update(WDT_BIT_SENSOR);
            }
        }

        reinit_ticks++;
        if (reinit_ticks == 40)
        {
            reinit_ticks = 0;
            check_sensor_health();
        }
        wdt_update(WDT_BIT_SENSOR);
    }
}
