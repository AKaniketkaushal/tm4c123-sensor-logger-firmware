#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driver/gpio.h"
#include "i2c.h"
#include "driver/sysctl.h"
#include "uart.h"
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
#include "sensor.h"
#include "my_debug.h"
#include "helper.h"
#include "BLE.h"
#include "main.h"

#define QUEUE_SEND_TIMEOUT  pdMS_TO_TICKS(50)
#define WDT_TIMEOUT_MS 300
#define STREAM_BUFFER_SIZE_BYTES (100)
#define STREAM_BUFFER_TRIGGER_LEVEL (1)

extern uint32_t SystemCoreClock;

QueueHandle_t Ble_commands;                            // Commands from BLE → SENSOR_TASK
QueueHandle_t Ble_responses;                           // Responses from SENSOR_TASK → BLE_SEND_TASK
QueueHandle_t i2c_isr_message;                         // Messages from I2C ISR → SENSOR_TASK
QueueHandle_t debug_queue;                             // For debug messages from various tasks → print_isr task
StreamBufferHandle_t uart1_str_buffer;                 // For UART1 ISR to send received data to BLE_RECEIVE_TASK
SemaphoreHandle_t t_mutex;                             // Mutex for UART access
const TickType_t buffer_wait_time = pdMS_TO_TICKS(50); // 100ms timeout

StreamBufferHandle_t uart_str_buffer;

TaskHandle_t ble_send_handle   = NULL;
TaskHandle_t ble_recv_handle   = NULL;
TaskHandle_t debug_console_handle = NULL;
TaskHandle_t sensor_handle     = NULL;
TaskHandle_t debug_handle      = NULL;
TaskHandle_t wdt_manager_handle = NULL;


void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Handle stack overflow - typically halt for debugging
    (void)xTask;
    (void)pcTaskName;
    // In debug: Set breakpoint here
    // In production: Log error, reset system, etc.
    while (1)
    {
        // Infinite loop to catch error
    }
}

// Malloc failed hook
void vApplicationMallocFailedHook(void)
{
    // Handle memory allocation failure

    // In debug: Set breakpoint here
    // In production: Log error, attempt recovery, etc.
    while (1)
    {
        // Infinite loop to catch error
    }
}





void system_init(void)
{
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL |
                   SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    SystemCoreClock = SysCtlClockGet();
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE))
        ;
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C2))
        ;
}

void HardFault_Handler(void)
 {
  char buffer[] = "HardFault_Handler...";
  char *p = buffer; // Create a pointer that can be incremented
  while(*p != '\0')
  {
    UARTCharPut(UART0_BASE, *p++);
  }
    while(1)
    {
        
    }
 }

int main(void)
{
   eeprom_init();
   system_init();
   uart_init();
    //  ble_init();
   /* Create ISR-dependent RTOS objects before enabling I2C2 IRQs. */
  

   uart_str_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL);
   Ble_commands = xQueueCreate(10, sizeof(char)); // Commands from BLE → SENSOR_TASK
   Ble_responses = xQueueCreate(10, sizeof(ble_msg));
   i2c_isr_message = xQueueCreate(10, sizeof(uint8_t));
   debug_queue = xQueueCreate(10, sizeof(uint8_t));
   uart1_str_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL); // For ISR to print messages via print_isr task
   t_mutex = xSemaphoreCreateMutex();

   configASSERT(uart_str_buffer  != NULL);
   configASSERT(uart1_str_buffer != NULL);
   configASSERT(Ble_commands     != NULL);
   configASSERT(Ble_responses    != NULL);
   configASSERT(i2c_isr_message  != NULL);
   configASSERT(debug_queue      != NULL);
   configASSERT(t_mutex          != NULL);

   i2c_init();
   xTaskCreate(BLE_SEND_TASK,       "BLE_SEND",      256, NULL, 1, &ble_send_handle);
   xTaskCreate(BLE_RECEIVE_TASK,    "BLE_RECV",      256, NULL, 1, &ble_recv_handle);
   xTaskCreate(DEBUG_CONSOLE_TASK,  "DBG_CONSOLE",   256, NULL, 1, &debug_console_handle);
   xTaskCreate(SENSOR_TASK,         "SENSOR",        512, NULL, 1, &sensor_handle);
   xTaskCreate(DEBUG_TASK,          "DEBUG",         256, NULL, 1, &debug_handle);
   xTaskCreate(WDT_MANAGER_TASK,    "WDT_MANAGER",   128, NULL, 1, &wdt_manager_handle);

    UARTPrint("Starting FreeRTOS Scheduler...\r\n");
    vTaskStartScheduler();
    while (1)
    {
        SysCtlDelay(SysCtlClockGet() / 3);
        UARTPrint("Starting FreeRTOS Scheduler...\r\n");
    }
    return 0;
}
