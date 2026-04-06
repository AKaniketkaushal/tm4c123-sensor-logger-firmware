/*
 * UART.c
 * UART driver for TM4C123GXL
 * UART0: Debug console (115200 baud) — PC communication
 * UART1: BLE module  (38400  baud) — HW-290/AT-09 communication
 */

#include "main.h"

/* ============================================================
 * Print Utility
 * ============================================================ */

void UARTPrint(const char *str)
{
    // Mutex-protected so multiple tasks don't interleave output
    if (xSemaphoreTake(t_mutex, 50) == pdTRUE)
    {
        while (*str)
            UARTCharPut(UART0_BASE, *str++);
        xSemaphoreGive(t_mutex);
    }
}

/* ============================================================
 * ISRs
 * ============================================================ */

void UART0_ISR(void)
{
    uint32_t status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, status);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Forward printable ASCII characters to DEBUG_CONSOLE_TASK via stream buffer
    while (UARTCharsAvail(UART0_BASE))
    {
        char c = UARTCharGetNonBlocking(UART0_BASE);
        if (c >= 0x20 && c <= 0x7E)
            xStreamBufferSendFromISR(uart1_str_buffer, &c, sizeof(c), &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void UART1_ISR(void)
{
    uint32_t status = UARTIntStatus(UART1_BASE, true);
    UARTIntClear(UART1_BASE, status);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Only process on RX data or receive timeout — avoids spurious triggers
    if (status & (UART_INT_RT | UART_INT_RX))
    {
        // Forward BLE data to BLE_RECEIVE_TASK via stream buffer
        while (UARTCharsAvail(UART1_BASE))
        {
            char c = UARTCharGetNonBlocking(UART1_BASE);
            if (c >= 0x20 && c <= 0x7E)
                xStreamBufferSendFromISR(uart_str_buffer, &c, sizeof(c), &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================
 * Init
 * ============================================================ */

void uart_init(void)
{
    // Enable peripheral clocks
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  // UART0 pins
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);  // UART1 pins + BLE status pin
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART1));

    // Pin mux: PA0=U0RX, PA1=U0TX, PB0=U1RX, PB1=U1TX
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);

    // PB2 = BLE connection status input (HIGH = connected)
    GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_2);

    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Baud rate config — disable before changing, re-enable after
    UARTDisable(UART0_BASE);
    UARTDisable(UART1_BASE);

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);

    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 38400,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);

    // Enable FIFOs — reduces ISR frequency, 1/8 threshold = interrupt at 2 bytes
    UARTFIFOEnable(UART0_BASE);
    UARTFIFOEnable(UART1_BASE);
    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);
    UARTFIFOLevelSet(UART1_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);

    // RX + receive timeout interrupts — RT catches partial packets at end of burst
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
    UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);

    // NVIC priority 6 — below FreeRTOS max syscall priority (5)
    NVIC_SetPriority(UART0_IRQn, 6);
    NVIC_SetPriority(UART1_IRQn, 6);

    NVIC_SetVector(UART0_IRQn, (uint32_t)&UART0_ISR);
    NVIC_EnableIRQ(UART0_IRQn);

    NVIC_SetVector(UART1_IRQn, (uint32_t)&UART1_ISR);
    NVIC_EnableIRQ(UART1_IRQn);

    UARTEnable(UART0_BASE);
    UARTEnable(UART1_BASE);
}
