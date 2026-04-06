#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sysctl.h"
#include "driver/uart.h"
#include <math.h>
#include <stdio.h>
#include "driver/rom.h"
#include "pin_map.h"
#include "TM4C123.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "stream_buffer.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "watchdog.h"
#include "my_uart.h"
#include "my_i2c.h"
#include "my_debug.h"
#include "BLE.h"
#include "helper.h"
#include "WDT.h"
#include "sensor.h"
#include "eeprom.h"

#endif // MAIN_H