/*
 * I2C.c
 * I2C driver for TM4C123GXL
 * Handles: init, read, write, bus recovery, ISR, scan
 */

#include "main.h"

/* ============================================================
 * ISR
 * ============================================================ */

static SemaphoreHandle_t i2c_mutex = NULL;

typedef void (*i2c_callback_t)(uint8_t event);
i2c_callback_t i2c_callback;

void I2C2_Handler(void)
{
    uint32_t status = I2CMasterIntStatusEx(I2C2_BASE, false); // read WITHOUT clearing
    uint32_t err    = I2CMasterErr(I2C2_BASE);                // read I2CMCS directly BEFORE clearing
    I2CMasterIntClearEx(I2C2_BASE, status);                   // NOW clear

    uint8_t event = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (status & I2C_MASTER_INT_DATA)
    {
        if      (err & I2C_MASTER_ERR_ADDR_ACK) event |= I2C_EVT_NACK_ADDR;
        else if (err & I2C_MASTER_ERR_DATA_ACK) event |= I2C_EVT_NACK_DATA;
        else                                     event |= I2C_DATA_TRANSMITTED;
    }

    if (status & I2C_MASTER_INT_TIMEOUT)  event |= I2C_EVT_CLK_TIMEOUT;
    if (status & I2C_MASTER_INT_ARB_LOST) event |= I2C_ARBITRATION_LOST;

    xQueueSendFromISR(i2c_isr_message, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}



void i2c_register_callback(i2c_callback_t callback)
{
    if (callback == NULL)
    {
        UARTPrint("Passing NULL callback to i2c_register_callback, ISR events will not be forwarded to callback\r\n");
    }
    i2c_callback = callback;
}

void my_i2c_isr_callback(uint8_t event)
{
    if (event & I2C_EVT_NACK_ADDR)
    {
        UARTPrint("I2C NACK on Address\r\n");
    }
    if (event & I2C_EVT_NACK_DATA)
    {
        UARTPrint("I2C NACK on Data\r\n");
    }
    if (event & I2C_EVT_CLK_TIMEOUT)
    {
        UARTPrint("I2C Clock Timeout\r\n");
    }
    if (event & I2C_ARBITRATION_LOST)
    {
        UARTPrint("I2C Arbitration Lost\r\n");
    }
}

/* ============================================================
 * Bus Recovery + Init
 * ============================================================ */

void revover_i2c_bus(void)
{
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_4);
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_5);
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_4, GPIO_PIN_4);

    for (int i = 0; i < 9; i++)
    {
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_4, 0);
        SysCtlDelay(SysCtlClockGet() / 100000);

        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_4, GPIO_PIN_4);
        SysCtlDelay(SysCtlClockGet() / 100000);

        if (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_5))
            break;
    }
}

void i2c_reinit()
{
    I2CMasterDisable(I2C2_BASE);
    revover_i2c_bus();
    GPIOPinConfigure(GPIO_PE4_I2C2SCL);
    GPIOPinConfigure(GPIO_PE5_I2C2SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTE_BASE, GPIO_PIN_4);
    GPIOPinTypeI2C(GPIO_PORTE_BASE, GPIO_PIN_5);
    I2CMasterIntEnableEx(I2C2_BASE, I2C_MASTER_INT_DATA | I2C_MASTER_INT_TIMEOUT | I2C_MASTER_INT_NACK);
    I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);
    I2CMasterEnable(I2C2_BASE);
}

i2c_status_t i2c_wait(void)
{
    uint8_t message;
    if (xQueueReceive(i2c_isr_message, &message, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        while(I2CMasterBusy(I2C2_BASE));
        if (i2c_callback)
        {
            i2c_callback(message);
        }
    
        if (message & (I2C_EVT_NACK_ADDR | I2C_EVT_NACK_DATA | I2C_EVT_CLK_TIMEOUT | I2C_ARBITRATION_LOST))
            return I2C_FAIL;

        return I2C_OK;
    }
    else
    {
        UARTPrint("I2C bus stuck, triggering recovery\r\n");
        i2c_reinit();
        return I2C_FAIL;
    }

}

void i2c_init(void)
{
    revover_i2c_bus();
    i2c_register_callback(my_i2c_isr_callback);
    i2c_mutex = xSemaphoreCreateMutex();
    GPIOPinConfigure(GPIO_PE4_I2C2SCL);
    GPIOPinConfigure(GPIO_PE5_I2C2SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTE_BASE, GPIO_PIN_4);
    GPIOPinTypeI2C(GPIO_PORTE_BASE, GPIO_PIN_5);

    I2CMasterIntEnableEx(I2C2_BASE, I2C_MASTER_INT_DATA |
                                        I2C_MASTER_INT_TIMEOUT |
                                        I2C_MASTER_INT_NACK);

    I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);
    I2CMasterEnable(I2C2_BASE);
    NVIC_SetPriority(I2C2_IRQn, 6);
    NVIC_SetVector(I2C2_IRQn, (uint32_t)&I2C2_Handler);
    NVIC_EnableIRQ(I2C2_IRQn);
}

/* ============================================================
 * Write Functions
 * ============================================================ */

 i2c_status_t i2c_write_addr(uint32_t i2c_base, uint8_t slave_address,
                    uint8_t reg_address, uint8_t data)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);

        I2CMasterDataPut(i2c_base, reg_address);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_START);
        status = i2c_wait();
        if(status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        I2CMasterDataPut(i2c_base, data);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_FINISH);
        status = i2c_wait();
        if(status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for write operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_write_string_addr(uint32_t i2c_base, uint8_t slave_address,
                                   uint8_t reg_address, const char *data, int length)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);
        I2CMasterDataPut(i2c_base, reg_address);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_START);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        for (int i = 0; i < length; i++)
        {
            I2CMasterDataPut(i2c_base, data[i]);
            if (i < length - 1)
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_CONT);
            else
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_FINISH);
            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }
        }
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for write operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_write_buffer_noaddr(uint32_t i2c_base, uint8_t slave_address, const uint8_t *data, uint32_t length)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);
        for (uint32_t i = 0; i < length; i++)
        {
            I2CMasterDataPut(i2c_base, data[i]);
            if (i == 0)
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_START);
            else if (i == length - 1)
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_FINISH);
            else
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_CONT);

            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }
        }
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for write operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_write_data_noaddr(uint32_t i2c_base, uint8_t slave_address, uint8_t data)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);
        I2CMasterDataPut(i2c_base, data);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_SINGLE_SEND);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            UARTPrint("I2C write failed\r\n");
            xSemaphoreGive(i2c_mutex);
            return status;
        }
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for write operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

/* ============================================================
 * Read Functions
 * ============================================================ */

i2c_status_t i2c_read_byte_addr(uint32_t i2c_base, uint8_t slave_address,
                        uint8_t reg_address, uint8_t *data)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);
        I2CMasterDataPut(i2c_base, reg_address);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_START);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        I2CMasterSlaveAddrSet(i2c_base, slave_address, true);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_SINGLE_RECEIVE);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        *data = I2CMasterDataGet(i2c_base);

        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for read operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_read_buffer_addr(uint32_t i2c_base, uint8_t slave_address, uint8_t reg_address, uint8_t *data, uint32_t length)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, false);
        I2CMasterDataPut(i2c_base, reg_address);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_SEND_START);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        I2CMasterSlaveAddrSet(i2c_base, slave_address, true);

        if (length == 1)
        {
            I2CMasterControl(i2c_base, I2C_MASTER_CMD_SINGLE_RECEIVE);
            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }

            data[0] = I2CMasterDataGet(i2c_base);
        }
        else
        {
            I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_START);
            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }

            data[0] = I2CMasterDataGet(i2c_base);

            for (uint32_t i = 1; i < length - 1; i++)
            {
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
                status = i2c_wait();
                if (status != I2C_OK)
                {
                    xSemaphoreGive(i2c_mutex);
                    return status;
                }

                data[i] = I2CMasterDataGet(i2c_base);
            }

            I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }

            data[length - 1] = I2CMasterDataGet(i2c_base);
        }
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for read operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_read_data_noaddr(uint32_t i2c_base, uint8_t slave_address, uint8_t *data)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, true);
        I2CMasterControl(i2c_base, I2C_MASTER_CMD_SINGLE_RECEIVE);
        status = i2c_wait();
        if (status != I2C_OK)
        {
            xSemaphoreGive(i2c_mutex);
            return status;
        }

        *data = I2CMasterDataGet(i2c_base);
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for read operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

i2c_status_t i2c_read_buffer_noaddr(uint32_t i2c_base, uint8_t slave_address, uint8_t *data, uint32_t length)
{
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdPASS)
    {
        i2c_status_t status;
        I2CMasterSlaveAddrSet(i2c_base, slave_address, true);
        for (int i = 0; i < (int)length; i++)
        {
            if (i == 0)
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_START);
            else if (i == length - 1)
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
            else
                I2CMasterControl(i2c_base, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
            status = i2c_wait();
            if (status != I2C_OK)
            {
                xSemaphoreGive(i2c_mutex);
                return status;
            }

            data[i] = I2CMasterDataGet(i2c_base);
        }
        xSemaphoreGive(i2c_mutex);
    }
    else
    {
        UARTPrint("Failed to acquire I2C mutex for read operation\r\n");
        return I2C_FAIL;
    }
    return I2C_OK;
}

/* ============================================================
 * Utility / Debug
 * ============================================================ */

void i2c_scan(void)
{
    UARTPrint("Scanning I2C bus...\r\n");
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(50)) != pdPASS)
            continue;

        I2CMasterSlaveAddrSet(I2C2_BASE, addr, false);
        I2CMasterDataPut(I2C2_BASE, 0x00);
        I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_SEND);
        i2c_wait(); // NACK on missing device is expected — callback will print it
        

       if (I2CMasterErr(I2C2_BASE) == I2C_MASTER_ERR_NONE)
        {
            char buf[30];
            snprintf(buf, sizeof(buf), "Device at: 0x%02X\r\n", addr);
            UARTPrint(buf);
        }
        xSemaphoreGive(i2c_mutex);
    }
    UARTPrint("Scan complete\r\n");
}

void probe_unknown_devices(void)
{
    char buf[60];
    uint8_t val = 0;
    uint8_t regs[] = {0x00, 0x01, 0x02, 0x09, 0x0A, 0x0C, 0x0D, 0x0F};

    UARTPrint("=== Probing 0x2C ===\r\n");
    for (int i = 0; i < 8; i++)
    {
        val = 0x00;
        i2c_read_byte_addr(I2C2_BASE, 0x2C, regs[i], &val);
        snprintf(buf, sizeof(buf), "0x2C reg[0x%02X] = 0x%02X\r\n", regs[i], val);
        UARTPrint(buf);
    }

    UARTPrint("=== Full address sweep with reg[0x00] read ===\r\n");
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        val = 0x00;
        i2c_read_byte_addr(I2C2_BASE, addr, 0x00, &val);
        if (val != 0xFF && val != 0x00)
        {
            snprintf(buf, sizeof(buf), "Addr 0x%02X reg[0x00] = 0x%02X\r\n", addr, val);
            UARTPrint(buf);
        }
    }
    UARTPrint("=== Sweep complete ===\r\n");
}
