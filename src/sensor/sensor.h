/*
 * SENSOR.h
 * Header for SENSOR.c
 * Declares sensor structs, register addresses, mode enum,
 * extern data instances, and all sensor function prototypes.
 */

#ifndef SENSOR_H
#define SENSOR_H


/* ============================================================
 * Register & Address Definitions
 * ============================================================ */

// BMP180 — Temperature & Pressure
#define BMP_180_ADDR    0x77

// QMC5883L / HMC5883L — Magnetometer
#define QMC5883L_ADDR   0x0D
#define HMC5883L_ADDR   0x1E
#define MAG_ADDR        0x2C
#define MAG_CTRL_REG1   0x09
#define MAG_CTRL_REG2   0x0A
#define MAG_FBR_REG     0x0B

// MPU6050 — Accelerometer & Gyroscope
#define MPU6050_ADDR    0x68    // AD0 = GND
#define PWR_MGMT_1      0x6B
#define SMPLRT_DIV      0x19
#define CONFIG_REG      0x1A
#define GYRO_CONFIG     0x1B
#define ACCEL_CONFIG    0x1C
#define INT_ENABLE      0x38
#define ACCEL_XOUT_H    0x3B
#define GYRO_XOUT_H     0x43
#define EEPROM_QUEUE_LENGTH 30

/* ============================================================
 * Sensor Mode Enum
 * ============================================================ */

typedef enum
{
    SENSOR_MODE_IDLE,
    SENSORA,            // BMP180 — one shot
    SENSORB,            // MPU6050 — one shot
    SENSORC,            // HMC5883L — one shot
    SENSOR_ALL,         // all sensors — one shot
    SENSORA_CONT,       // BMP180 — continuous
    SENSORB_CONT,       // MPU6050 — continuous
    SENSORC_CONT,       // HMC5883L — continuous
    SENSOR_ALL_CONT     // all sensors — continuous
} sensor_modes_t;

/* ============================================================
 * Sensor Data Structs
 * ============================================================ */

typedef struct
{
    unsigned char data[22];     // raw factory calibration coefficients
    float         temperature;  // degrees Celsius
    uint32_t      pressure;     // Pascals
    bool          is_calibrated;
} bmp180_t;

typedef struct
{
    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;
    float heading;
    float heading_deg;
    int16_t x_calib;
    int16_t y_calib;
    int16_t z_calib;
} HMC5883L_data_t;


typedef struct {
    uint8_t  type;
    uint8_t  sensor_mode;
    uint8_t  checksum;
    uint8_t  _pad;
    uint32_t timestamp;

    float    temperature;       
    float    pressure;          

    float    accel_x;           
    float    accel_y;
    float    accel_z;
    float    gyro_x;
    float    gyro_y;
    float    gyro_z;

    float    heading_deg;        
    float    _mag_pad;

}EepromRecord_t;  

typedef struct{
uint16_t start;
uint16_t end;
uint16_t count;
uint16_t magic;
}eeprom_metadata_t;

typedef struct
{
    int16_t accel_x, accel_y, accel_z;         // raw ADC counts
    int16_t gyro_x,  gyro_y,  gyro_z;
    float   accel_x_g,  accel_y_g,  accel_z_g; // converted to g
    float   gyro_x_dps, gyro_y_dps, gyro_z_dps;// converted to °/s
    float   accl_calib_x, accl_calib_y, accl_calib_z; // zero-offset bias
    float   gyro_calib_x, gyro_calib_y, gyro_calib_z;
} MPU_6050_data_t;

/* ============================================================
 * Extern Data Instances
 * Defined once in SENSOR.c — shared across all translation units
 * ============================================================ */

extern bmp180_t        bmp180;
extern HMC5883L_data_t hmc5883l;
extern MPU_6050_data_t mpu6050_data;


/* ============================================================
 * Extern Queue Handle
 * Defined in main.c, used by send_*_ble() functions
 * ============================================================ */

extern QueueHandle_t Ble_commands;
extern QueueHandle_t Ble_responses;


typedef enum{
REC_TYPE_TEMP_PRESSURE,
REC_TYPE_GYRO,
REC_TYPE_MAGNETOMETER,
REC_TYPE_ALL_SENSORS,
REC_TYPE_TEMP_PRESSURE_CONT,
REC_TYPE_GYRO_CONT,
REC_TYPE_MAGNETOMETER_CONT,
REC_TYPE_ALL_SENSORS_CONT
}record_type_t;

/* ============================================================
 * Function Declarations
 * ============================================================ */

// FreeRTOS task — inits sensors, dispatches reads on BLE command
void SENSOR_TASK(void *pvParameters);
void eeprom_init(void);

// BMP180
void bmp180_init(void);
void get_calibration_data(void);
bool read_bmp180_data(void);

// MPU6050
void MPU_6050_init(void);
bool read_MPU_6050(void);
bool calibrate_mpu6050(void);

// HMC5883L
void HMC5883L_init(void);
bool read_HMC5883L(void);

// BLE data senders — called by SENSOR_TASK and DEBUG_TASK
void send_temp_pressure_ble(void);
void send_gyro_data_ble(void);
void send_magnetometer_data_ble(void);

// Mode dispatcher — re-runs last continuous mode
void switch_existing_mode(uint8_t mode);

#endif // SENSOR_H
