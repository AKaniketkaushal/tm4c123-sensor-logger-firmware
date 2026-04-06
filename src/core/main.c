#include "main.h"   

SemaphoreHandle_t t_mutex; // Mutex for UART access






TaskHandle_t wdt_manager_handle = NULL;

extern uint32_t SystemCoreClock;
QueueHandle_t Ble_commands;                            // Commands from BLE → SENSOR_TASK
QueueHandle_t Ble_responses;                           // Responses from SENSOR_TASK → BLE_SEND_TASK
QueueHandle_t i2c_isr_message;                         // Messages from I2C ISR → SENSOR_TASK
QueueHandle_t debug_queue;                             // For debug messages from various tasks → print_isr task
StreamBufferHandle_t uart1_str_buffer;                 // For UART1 ISR to send received data to BLE_RECEIVE_TASK
const TickType_t buffer_wait_time = pdMS_TO_TICKS(50); // 100ms timeout

static char uart_buffer[40];
static int uart_buffer_index = 0;
static bool uart_buffer_received = false;
#define STREAM_BUFFER_SIZE_BYTES (100)
#define STREAM_BUFFER_TRIGGER_LEVEL (1)

StreamBufferHandle_t uart_str_buffer;





TaskHandle_t ble_send_handle   = NULL;
TaskHandle_t ble_recv_handle   = NULL;
TaskHandle_t debug_console_handle = NULL;
TaskHandle_t sensor_handle     = NULL;
TaskHandle_t debug_handle      = NULL;



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
    UARTPrint("HardFault_Handler: A hard fault has occurred!\r\n");
    while(1)
    {
    }
 }

// int main(void)
// {
//    eeprom_init();
//    system_init();
//    uart_init();
//    i2c_init();
//   //  ble_init(); // Called initilly to set baud rate to 38400, will be reconfigured in BLE_SEND_TASK based on debug commands
    
//    uart_str_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL);
//    Ble_commands = xQueueCreate(10, sizeof(char)); // Commands from BLE → SENSOR_TASK
//    Ble_responses = xQueueCreate(10, sizeof(ble_msg));
//    i2c_isr_message = xQueueCreate(10, sizeof(char) * 10);
//    debug_queue = xQueueCreate(10, sizeof(char));
//    uart1_str_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL); // For ISR to print messages via print_isr task
//    t_mutex = xSemaphoreCreateMutex();
//    print_mutex = xSemaphoreCreateMutex();
//    xTaskCreate(BLE_SEND_TASK,       "BLE_SEND",      256, NULL, 1, &ble_send_handle);
//    xTaskCreate(BLE_RECEIVE_TASK,    "BLE_RECV",      256, NULL, 1, &ble_recv_handle);
//    xTaskCreate(DEBUG_CONSOLE_TASK,  "DBG_CONSOLE",   256, NULL, 1, &debug_console_handle);
//    xTaskCreate(SENSOR_TASK,         "SENSOR",        512, NULL, 2, &sensor_handle);
//    xTaskCreate(DEBUG_TASK,          "DEBUG",         256, NULL, 1, &debug_handle);
//    xTaskCreate(WDT_MANAGER_TASK,    "WDT_MANAGER",   128, NULL, 1, &wdt_manager_handle);

//     UARTPrint("Starting FreeRTOS Scheduler...\r\n");
//     UARTPrint("System initialized successfully\r\n");
//     vTaskStartScheduler();
//     while (1)
//     {
//         SysCtlDelay(SysCtlClockGet() / 3);
//         UARTPrint("Starting FreeRTOS Scheduler...\r\n");
//     }
//     return 0;
// }
int main(void)
{
    /* =========================================================
     * STEP 1 — Hardware init (no FreeRTOS objects exist yet,
     *           no ISRs that touch queues should fire here)
     * ========================================================= */
    eeprom_init();
    system_init();
    uart_init();

    /* =========================================================
     * STEP 2 — Create ALL FreeRTOS objects BEFORE any peripheral
     *           that enables an IRQ which calls FromISR APIs.
     *
     *           CRITICAL: i2c_isr_message MUST exist before
     *           i2c_init() calls NVIC_EnableIRQ(I2C2_IRQn).
     *           A NULL queue handle in xQueueSendFromISR = HardFault.
     * ========================================================= */
    uart_str_buffer  = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES,
                                           STREAM_BUFFER_TRIGGER_LEVEL);
    uart1_str_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE_BYTES,
                                           STREAM_BUFFER_TRIGGER_LEVEL);

    Ble_commands    = xQueueCreate(10, sizeof(uint8_t));     // sensor_modes_t fits in uint8_t
    Ble_responses   = xQueueCreate(10, sizeof(ble_msg));
    i2c_isr_message = xQueueCreate(10, sizeof(uint8_t));     // FIX: item size = 1 byte, NOT sizeof(char)*10
    debug_queue     = xQueueCreate(10, sizeof(uint8_t));

    t_mutex     = xSemaphoreCreateMutex();

    /* Catch allocation failures early — never silently proceed
       with a NULL handle into ISR territory                    */
    configASSERT(uart_str_buffer  != NULL);
    configASSERT(uart1_str_buffer != NULL);
    configASSERT(Ble_commands     != NULL);
    configASSERT(Ble_responses    != NULL);
    configASSERT(i2c_isr_message  != NULL);  // most critical
    configASSERT(debug_queue      != NULL);
    configASSERT(t_mutex          != NULL);
    

    /* =========================================================
     * STEP 3 — NOW safe to init I2C.
     *           i2c_isr_message exists, so I2C2_Handler can fire
     *           without crashing.
     * ========================================================= */
    i2c_init();

    /* =========================================================
     * STEP 4 — BLE init (if it also enables an IRQ, it belongs
     *           here after its queue/semaphore deps are ready)
     * ========================================================= */
    // ble_init();

    /* =========================================================
     * STEP 5 — Create tasks
     * ========================================================= */
    xTaskCreate(BLE_SEND_TASK,      "BLE_SEND",    256, NULL, 1, &ble_send_handle);
    xTaskCreate(BLE_RECEIVE_TASK,   "BLE_RECV",    256, NULL, 1, &ble_recv_handle);
    xTaskCreate(DEBUG_CONSOLE_TASK, "DBG_CONSOLE", 256, NULL, 1, &debug_console_handle);
    xTaskCreate(SENSOR_TASK,        "SENSOR",      512, NULL, 2, &sensor_handle);
    xTaskCreate(DEBUG_TASK,         "DEBUG",       512, NULL, 1, &debug_handle);
    xTaskCreate(WDT_MANAGER_TASK,   "WDT_MANAGER", 128, NULL, 1, &wdt_manager_handle);

    UARTPrint("Starting FreeRTOS Scheduler...\r\n");
    vTaskStartScheduler();

    /* Should never reach here */
    while (1);
    return 0;
}
