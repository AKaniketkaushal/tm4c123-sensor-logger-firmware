
#include "main.h"


uint8_t wdt_status_bits = 0;

void WDT_ISR(void)
{
    WatchdogIntClear(WATCHDOG0_BASE);

    uint8_t snapshot = wdt_status_bits;

    if (!(snapshot & WDT_BIT_BLE_SEND))    UARTPrint("WDT: BLESEND starved\r\n");
    if (!(snapshot & WDT_BIT_BLE_RECV))    UARTPrint("WDT: BLERECV starved\r\n");
    if (!(snapshot & WDT_BIT_SENSOR))     UARTPrint("WDT: SENSOR starved\r\n");
    if (!(snapshot & WDT_BIT_DBG_CONSOLE)) UARTPrint("WDT: DBGCONSOLE starved\r\n");
    if (!(snapshot & WDT_BIT_DEBUG))      UARTPrint("WDT: DEBUG starved\r\n");
}


void wdt_update(uint8_t bit)
{
    taskENTER_CRITICAL();
    wdt_status_bits |= bit;
    taskEXIT_CRITICAL();
}

void wdt_kick()
{
    WatchdogIntClear(WATCHDOG0_BASE);
}

void wdt_init()
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_WDOG0));
    
    if(WatchdogLockState(WATCHDOG0_BASE) == true)
    {
        WatchdogUnlock(WATCHDOG0_BASE);
    }
    WatchdogStallEnable(WATCHDOG0_BASE); 
    WatchdogResetEnable(WATCHDOG0_BASE);
    WatchdogReloadSet(WATCHDOG0_BASE, (SysCtlClockGet()/1000)*WDT_TIMEOUT_MS); 
    WatchdogEnable(WATCHDOG0_BASE);
    WatchdogLock(WATCHDOG0_BASE);
}

void WDT_MANAGER_TASK(void *pvParameters)
{
    wdt_init();
    while(1)
    {
        taskENTER_CRITICAL();
        uint8_t snapshot = wdt_status_bits;
        wdt_status_bits = 0;           
        taskEXIT_CRITICAL();

        if(snapshot == ALL_BITS_WDT)
        {
            wdt_kick();
        }
        else
        {
          //  UARTPrint("Tasks in blocked state initializing system reset\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

