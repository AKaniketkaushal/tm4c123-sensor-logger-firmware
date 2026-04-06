/*
 * I2C.h
 * Header for I2C.c
 * Peripheral : I2C2 on PE4 (SCL) / PE5 (SDA)
 * Declares I2C event enum, queue handle extern,
 * and all I2C driver function prototypes.
 */

#ifndef MY_I2C_H
#define MY_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

/* ============================================================
 * I2C Event Enum
 * Bit flags sent from I2C2_Handler ISR → i2c_isr_message queue
 * Consumed by DEBUG_CONSOLE_TASK to print I2C errors
 * ============================================================ */

typedef enum
{
    I2C_DATA_TRANSMITTED = (1 << 0),  // 0x01 — data byte ACKed by slave
    I2C_EVT_NACK_ADDR    = (1 << 1),  // 0x02 — slave did not ACK its address
    I2C_EVT_NACK_DATA    = (1 << 2),  // 0x04 — slave did not ACK a data byte
    I2C_EVT_CLK_TIMEOUT  = (1 << 3),  // 0x08 — SCL held low too long
    I2C_ARBITRATION_LOST = (1 << 4),  // 0x10 — multi-master bus contention
} I2C_Event;

/* ============================================================
 * Extern Queue Handle
 * Defined in main.c — I2C2_Handler sends events, DEBUG_CONSOLE_TASK reads them
 * ============================================================ */

extern QueueHandle_t i2c_isr_message;

/* ============================================================
 * Init & Bus Recovery
 * ============================================================ */

// Bit-bangs 9 SCL pulses to unstick a slave holding SDA low
void revover_i2c_bus(void);

// Configures I2C2 pins, clock, interrupts, and NVIC
void i2c_init(void);

typedef enum{
    I2C_FAIL = 0,
    I2C_OK
} i2c_status_t;

/* ============================================================
 * Write Functions
 * ============================================================ */

// Write single byte to a register address  — START | addr | reg | data | STOP
i2c_status_t i2c_write_addr(uint32_t i2c_base, uint8_t slave_address,
                    uint8_t reg_address, uint8_t data);

// Write string to a register address — START | addr | reg | data[0..n] | STOP
i2c_status_t i2c_write_string_addr(uint32_t i2c_base, uint8_t slave_address,
                           uint8_t reg_address, const char *data, int length);

// Write buffer without register address — START | addr | data[0..n] | STOP
i2c_status_t i2c_write_buffer_noaddr(uint32_t i2c_base, uint8_t slave_address,
                                     const uint8_t *data, uint32_t length);

// Write single byte without register address — START | addr | data | STOP
i2c_status_t i2c_write_data_noaddr(uint32_t i2c_base, uint8_t slave_address,
                                   uint8_t data);

/* ============================================================
 * Read Functions
 * ============================================================ */

// Read single byte from a register — START | addr | reg | RESTART | addr | data | STOP
i2c_status_t i2c_read_byte_addr(uint32_t i2c_base, uint8_t slave_address,
                                uint8_t reg_address, uint8_t *data);

// Read multiple bytes from a register — burst receive with NACK+STOP on last byte
i2c_status_t i2c_read_buffer_addr(uint32_t i2c_base, uint8_t slave_address,
                                  uint8_t reg_address, uint8_t *data, uint32_t length);

// Read single byte without register address — START | addr | data | STOP
i2c_status_t i2c_read_data_noaddr(uint32_t i2c_base, uint8_t slave_address,
                                  uint8_t *data);

// Read multiple bytes without register address — burst receive
i2c_status_t i2c_read_buffer_noaddr(uint32_t i2c_base, uint8_t slave_address, uint8_t *data, uint32_t length);

/* ============================================================
 * ISR
 * ============================================================ */

// I2C2 interrupt handler — sends event flags to i2c_isr_message queue
void I2C2_Handler(void);

/* ============================================================
 * Utility / Debug
 * ============================================================ */

// Scans 0x08–0x77, prints address of every responding device
void i2c_scan(void);

// Probes 0x2C registers and sweeps full address range for non-trivial responses
void probe_unknown_devices(void);



#endif // I2C_H
