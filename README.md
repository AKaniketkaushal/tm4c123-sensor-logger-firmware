# TM4C123 Sensor Logger Firmware

![Platform](https://img.shields.io/badge/platform-TM4C123GXL-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C11-informational?style=flat-square)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-00a884?style=flat-square)
![Build](https://img.shields.io/badge/build-CMake-orange?style=flat-square)
![Toolchain](https://img.shields.io/badge/toolchain-arm--none--eabi-critical?style=flat-square)

FreeRTOS-based firmware for the TI TM4C123GXL LaunchPad that reads multiple sensors, streams telemetry over BLE, exposes a UART debug console, logs records to internal EEPROM, and supervises task liveness with a software watchdog layered over the TM4C123 hardware watchdog.

## Overview

This project is a practical embedded firmware project focused on:

- real hardware bring-up on TM4C123
- RTOS task coordination
- queue- and stream-buffer-based communication
- multi-sensor I2C integration
- BLE and UART command handling
- EEPROM-backed data logging
- watchdog-aware runtime supervision

The current firmware supports both one-shot and continuous sensor acquisition flows, with continuous sampling driven by FreeRTOS software timers and task notifications inside the sensor subsystem.

## Project Highlights

- custom CMake-based embedded build setup instead of relying on an IDE-generated project
- custom startup/runtime integration using CMSIS startup code and project-specific fault handling
- FreeRTOS-based task architecture with queues, stream buffers, timers, and task notifications
- custom TM4C123 UART and I2C driver integration
- EEPROM-backed sensor logging with watchdog-supervised task liveness

## Supported Sensors

- BMP180 for temperature and pressure
- MPU6050 for accelerometer and gyroscope data
- QMC5883L/HMC5883L-compatible magnetometer on the shared I2C bus

## Hardware

- TI TM4C123GXL LaunchPad
- BMP180
- MPU6050
- magnetometer module on I2C2
- BLE UART module such as HW-290 / AT-09 class modules

## Software Stack

- C11
- FreeRTOS
- TivaWare
- CMSIS
- CMake
- GNU Arm Embedded Toolchain (`arm-none-eabi`)

## Runtime Architecture

The firmware is organized around these FreeRTOS tasks:

- `BLE_SEND_TASK`: dequeues BLE responses and transmits them over UART1
- `BLE_RECEIVE_TASK`: parses UART1 input into sensor commands
- `DEBUG_CONSOLE_TASK`: parses UART0 debug commands
- `DEBUG_TASK`: executes debug actions and diagnostics
- `SENSOR_TASK`: owns sensor reads, continuous sampling flow, and EEPROM logging
- `WDT_MANAGER_TASK`: validates task liveness bits and refreshes the hardware watchdog

The main task communication primitives used in the code are:

- queues for command and response dispatch
- stream buffers for UART ISR-to-task data transfer
- task notifications for timer-driven sensor send/store events
- mutex-protected UART printing

## System Diagram


```text
================================================================================
                        TM4C123 SENSOR LOGGER SYSTEM
================================================================================

                           DEBUG / UART1 PATH

                         +----------------------------+
                         |        PC TERMINAL         |
                         |           UART1            |
                         +-------------+--------------+
                                       |
                              UART1 ISR (debug RX)
                                       |
                    Stream Buffer (uart_debug_rx_buffer)
                                       |
                                       v
                    +--------------------------------+
                    | DEBUG_CONSOLE_TASK             |
                    | - parse debug commands         |
                    | - send work to debug_queue     |
                    +---------------+----------------+
                                    |
                                    v
                             +--------------+
                             | DEBUG_TASK   |
                             | diagnostics  |
                             +------+-------+
                                    |
                                    | 
                                    v
                    direct diagnostic sensor access



                            BLE / UART0 PATH
                         +----------------------------+
                         |        BLE MODULE          |
                         |           UART0            |
                         +-------------+--------------+
                                       |
                               UART0 ISR (BLE RX)
                                       |
                      Stream Buffer (uart_ble_rx_buffer)
                                       |
                                       v
                        +------------------------------+
                        | BLE_RECEIVE_TASK             |
                        | - parse BLE commands         |
                        +--------------+---------------+
                                       |
                                       | Queue: Ble_commands
                                       v
                             +----------------+
                             |  SENSOR_TASK   |
                             |  Core engine   |
                             +-------+--------+
                                     |
        +----------------------------+----------------------------+
        |                            |                            |
        v                            v                            v
 +---------------+         +----------------+           +----------------+
 | Sensor Read   |         | Data Format    |           | EEPROM         |
 | + Drivers     |         | + BLE Packet   |           | Logging        |
 +-------+-------+         +--------+-------+           +--------+-------+
         |                          |                            |
         v                          v                            v
 +---------------+         +----------------+           +----------------+
 | BMP180        |         | BLE_SEND_TASK  |           | EEPROM HW      |
 | MPU6050       |         | UART1 TX       |           | Internal       |
 | HMC5883L      |         +----------------+           +----------------+
 +-------+-------+
         |
         v
 +---------------+
 | I2C DRIVER    |
 | I2C2          |
 +-------+-------+
         |
         v
 +---------------+
 | I2C2 ISR      |
 | + event queue |
 +---------------+


================================================================================
                         TIMER + EVENT SYSTEM
================================================================================

   SEND TIMER  -> SENSOR_TASK (notify SEND)
   STORE TIMER -> SENSOR_TASK (notify STORE)

================================================================================
                         WATCHDOG SYSTEM
================================================================================

   All tasks -> wdt_update() -> WDT_MANAGER_TASK -> Hardware Watchdog
```

## Sensor Flow

The sensor subsystem supports two styles of operation:

- one-shot commands such as reading a sensor once and optionally storing the record
- continuous modes where periodic BLE sends and EEPROM stores are scheduled separately

In the current implementation:

- a FreeRTOS software timer drives periodic telemetry sends
- a second timer drives slower EEPROM logging
- `SENSOR_TASK` receives timer notifications and processes the active sensor mode
- sensor health is monitored and failed devices are re-initialized periodically

## Data Flow Summary

BLE flow:

- BLE module -> UART0 ISR -> stream buffer -> `BLE_RECEIVE_TASK`
- `BLE_RECEIVE_TASK` -> `Ble_commands` queue -> `SENSOR_TASK`
- `SENSOR_TASK` -> format response -> `BLE_SEND_TASK` -> UART0 TX

Debug flow:

- PC terminal -> UART0 ISR -> debug RX stream buffer -> `DEBUG_CONSOLE_TASK`
- `DEBUG_CONSOLE_TASK` -> `debug_queue` -> `DEBUG_TASK`
- `DEBUG_TASK` can trigger direct diagnostic sensor actions

I2C flow:

- `SENSOR_TASK` -> I2C driver -> I2C2 ISR / event queue -> sensor transaction result

Storage flow:

- `SENSOR_TASK` -> EEPROM record creation -> internal EEPROM circular buffer

## Peripherals and Interfaces

- `UART0`: BLE module interface
- `UART1`: debug console input/output
- `I2C2`: shared sensor bus
- internal EEPROM: circular record storage

## Project Structure

- [src/core](src/core): startup, system init, RTOS object creation, task creation
- [src/drivers](src/drivers): UART and I2C drivers
- [src/sensor](src/sensor): sensor init, acquisition, calibration, timer-driven sampling, EEPROM record handling
- [src/ble](src/ble): BLE send/receive tasks and command dispatch
- [src/debug](src/debug): debug console command parsing and diagnostics
- [src/wdt](src/wdt): watchdog manager and task heartbeat tracking
- [src/utils](src/utils): helper and formatting utilities
- [src/middleware](src/middleware): middleware configuration such as FreeRTOS config
- [linker](linker): linker script
- [Freertos](Freertos): FreeRTOS kernel sources
- [CMSIS](CMSIS): CMSIS and device support files
- [tivaware](tivaware): TivaWare driver library sources

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

- [CMakeLists.txt](CMakeLists.txt)
- [toolchain-arm-none-eabi.cmake](toolchain-arm-none-eabi.cmake)

This project uses a hand-configured CMake build flow rather than an autogenerated IDE build, which keeps the build setup explicit and easier to reason about.

## Startup and Runtime Bring-Up

The firmware uses CMSIS startup and system initialization sources together with project-defined runtime behavior:

- startup entry is provided through [startup.c](CMSIS/Device/Source/startup.c)
- system initialization hooks are provided through [system_TM4C123.c](CMSIS/Device/Source/system_TM4C123.c)
- the application defines its own [HardFault_Handler](src/core/main.c) and startup flow in [main.c](src/core/main.c)

This setup reflects deliberate ownership of the build and startup path rather than a default vendor IDE scaffold.

## Debug Commands

The firmware currently includes debug commands such as:

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

Command parsing and execution live primarily in [my_debug.c](src/debug/my_debug.c).

## What This Project Demonstrates

- RTOS task design on a Cortex-M microcontroller
- ISR-to-task communication using stream buffers and queues
- timer-driven periodic work using FreeRTOS timers and notifications
- multi-sensor I2C integration and bus recovery
- BLE/UART command-driven telemetry flow
- EEPROM-backed embedded logging
- watchdog-aware firmware design
- fault detection and sensor recovery behavior on real hardware

## Current Limitations

- some recovery paths still need more hardening and validation
- debug output is still fairly synchronous
- BLE command/response framing can be made more structured
- EEPROM record metadata and validation can be expanded further
- some implementation details are still being refined as the firmware evolves

## Roadmap

- tighten sensor recovery and retry behavior
- improve command protocol structure for BLE and debug interfaces
- move more logging and diagnostics off synchronous paths
- expand EEPROM metadata and record validation
- persist calibration/configuration data in EEPROM

## Showcase Summary

This project is intended to show practical embedded systems work rather than a minimal sensor demo. The focus is on task coordination, interrupt-to-task communication, sensor integration, telemetry flow, persistent logging, and robustness improvements on TM4C123 hardware.
