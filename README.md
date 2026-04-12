# TM4C123 Sensor Logger Firmware

FreeRTOS-based firmware for the TI TM4C123GXL LaunchPad that samples multiple sensors, streams data over BLE, supports a UART debug console, logs records to internal EEPROM, and supervises task liveness with a watchdog.

## Overview

This project is a practical embedded systems firmware project built around real hardware bring-up, RTOS task coordination, sensor integration, and fault-aware operation.

The firmware currently supports:

- BMP180 temperature and pressure sensing
- MPU6050 accelerometer and gyroscope sensing
- QMC5883L/HMC5883L-compatible magnetometer sensing
- BLE command and response handling over UART
- UART debug console commands
- EEPROM-backed sensor record storage
- Watchdog-based runtime supervision
- I2C communication and recovery support

## Hardware

- TI TM4C123GXL LaunchPad
- BMP180
- MPU6050
- Magnetometer module on the project I2C bus
- BLE UART module

## Software Stack

- C11
- FreeRTOS
- TivaWare
- CMSIS
- CMake
- `arm-none-eabi` toolchain

## Project Structure

- [src/core](/c:/tm4c123gxl/src/core): startup, RTOS object creation, task creation
- [src/drivers](/c:/tm4c123gxl/src/drivers): UART and I2C drivers
- [src/sensor](/c:/tm4c123gxl/src/sensor): sensor init, reads, calibration, EEPROM record handling
- [src/ble](/c:/tm4c123gxl/src/ble): BLE send/receive tasks and command handling
- [src/debug](/c:/tm4c123gxl/src/debug): debug console and diagnostics
- [src/wdt](/c:/tm4c123gxl/src/wdt): watchdog manager and liveness tracking
- [src/utils](/c:/tm4c123gxl/src/utils): helper and formatting utilities
- [linker](/c:/tm4c123gxl/linker): linker scripts
- [Freertos](/c:/tm4c123gxl/Freertos): FreeRTOS kernel sources
- [CMSIS](/c:/tm4c123gxl/CMSIS): CMSIS headers and device support
- [tivaware](/c:/tm4c123gxl/tivaware): TivaWare driver library sources

## Architecture

The firmware is organized around FreeRTOS tasks for:

- BLE transmit
- BLE receive
- debug console processing
- sensor sampling and command handling
- watchdog supervision

Commands arrive through BLE or UART, are dispatched through queues, and are processed by the appropriate task. Sensor results can be:

- transmitted live over BLE
- printed over the debug UART
- stored in internal EEPROM as records

The sensor subsystem supports both one-shot and continuous sampling behavior, while the watchdog subsystem monitors task health and refreshes the hardware watchdog only when expected tasks remain responsive.

## Build

Build with CMake and the ARM GCC toolchain:

```powershell
cmake -S . -B build
cmake --build build
```

Generated artifacts include:

- `tm4c123gxl.elf`
- `tm4c123gxl.bin`
- `tm4c123gxl.hex`

Relevant build files:

- [CMakeLists.txt](/c:/tm4c123gxl/CMakeLists.txt)
- [toolchain-arm-none-eabi.cmake](/c:/tm4c123gxl/toolchain-arm-none-eabi.cmake)

## Runtime Notes

- EEPROM is initialized during startup
- I2C2 is used for the sensor bus
- UART0 is used for debug input/output
- UART1 is used for the BLE module

## Debug Commands

Example commands supported by the firmware include:

- `BLE_CHECK_CONNECTION`
- `I2C_SCAN`
- `READ_SENSORA`
- `READ_SENSORB`
- `READ_SENSORC`
- `READ_ALL_SENSORS`
- `SYSTEM_STATUS`
- `display_eeprom`
- `CALIBRATE_HMC5883L`
- `CALIB_STOP`

Command handling is implemented in [my_debug.c](/c:/tm4c123gxl/src/debug/my_debug.c).

## What This Project Demonstrates

- RTOS task design on a Cortex-M microcontroller
- queue- and stream-buffer-based task communication
- multi-sensor I2C integration
- BLE/UART command handling
- EEPROM-backed embedded logging
- watchdog-aware firmware design
- fault detection and recovery-oriented embedded development

## Current Limitations

- some sensor recovery paths still need hardening
- debug output and logging are still fairly synchronous
- EEPROM record metadata can be expanded further
- BLE command/response framing can be made more structured

## Roadmap

- improve sensor recovery and retry behavior
- move toward more structured periodic/event-driven scheduling
- make logging more asynchronous
- extend EEPROM metadata and record validation
- persist calibration and configuration data

## Showcase Summary

This project is meant to demonstrate practical embedded firmware work rather than a minimal demo application. The focus is on real-device integration, RTOS-based coordination, telemetry flow, persistent logging, and robustness improvements on TM4C123 hardware.
