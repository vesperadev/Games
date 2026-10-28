#include "main.h"
#include "i2c.h"
#include "cmsis_os2.h"
#include "I2CBus.h"
#include "TempTask.h"

/// Sensor: AHT20 / AHT10 compatible, 7-bit address 0x38 on I2C1.
/// If your board carries a different chip (e.g. SHT30 @ 0x44), only the
/// command bytes / conversion formulas below need to be replaced.
#define AHT2X_ADDR          (0x38 << 1)  ///< HAL wants the 8-bit address
#define AHT2X_CMD_INIT      0xBE         ///< initialise / calibrate
#define AHT2X_CMD_TRIGGER   0xAC         ///< start a measurement
#define AHT2X_TRIGGER_MS    80           ///< conversion time of one measurement
#define AHT2X_SCALE         1048576UL    ///< 2^20, the raw value is 20 bit
#define TEMP_PERIOD_MS      2000         ///< sampling period

static volatile TempData s_data;   ///< latest sample, shared with the UI
static volatile uint8_t  s_seq;    ///< incremented on every new sample
static uint8_t  s_readSeq;     ///< sequence already handed out to the UI

/// Write bytes to the sensor
static uint8_t AHT2X_Write(const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;

    I2C1Bus_Lock();
    st = HAL_I2C_Master_Transmit(&hi2c1, AHT2X_ADDR, (uint8_t *)buf, len, 50);
    I2C1Bus_Unlock();

    return (st == HAL_OK) ? 1 : 0;
}

/// Read bytes from the sensor
static uint8_t AHT2X_Read(uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st;

    I2C1Bus_Lock();
    st = HAL_I2C_Master_Receive(&hi2c1, AHT2X_ADDR, buf, len, 50);
    I2C1Bus_Unlock();

    return (st == HAL_OK) ? 1 : 0;
}

/// Power-up and make sure the sensor is calibrated (status bit 0x08)
static uint8_t AHT2X_Init(void)
{
    uint8_t status = 0;
    uint8_t cmd[3] = { AHT2X_CMD_INIT, 0x08, 0x00 };

    osDelay(40);   ///< sensor needs >= 40 ms after power on

    if (!AHT2X_Read(&status, 1)) {
        return 0;  ///< no ACK: wrong address or sensor not fitted
    }

    if ((status & 0x08) == 0) {
        /// not calibrated yet, run the init sequence once
        if (!AHT2X_Write(cmd, sizeof(cmd))) {
            return 0;
        }
        osDelay(10);
    }

    return 1;
}

/// Trigger one measurement and convert the raw values
static uint8_t AHT2X_ReadData(TempData *out)
{
    uint8_t  cmd[3] = { AHT2X_CMD_TRIGGER, 0x33, 0x00 };
    uint8_t  raw[7];
    uint32_t hum_raw;
    uint32_t temp_raw;

    if (!AHT2X_Write(cmd, sizeof(cmd))) {
        return 0;
    }

    osDelay(AHT2X_TRIGGER_MS);

    if (!AHT2X_Read(raw, sizeof(raw))) {
        return 0;
    }

    if (raw[0] & 0x80) {
        return 0;  ///< still busy, drop this sample
    }

    /// humidity: raw[1..3] (20 bit), temperature: raw[3..5] (20 bit)
    hum_raw  = ((uint32_t)raw[1] << 16) | ((uint32_t)raw[2] << 8) | raw[3];
    hum_raw >>= 4;
    temp_raw = (((uint32_t)raw[3] & 0x0F) << 16) | ((uint32_t)raw[4] << 8) | raw[5];

    /// humidity = raw / 2^20 * 100 [%]
    /// temp     = raw / 2^20 * 200 - 50 [degC]
    out->hum_x10  = (int16_t)((hum_raw * 1000UL) / AHT2X_SCALE);
    out->temp_x10 = (int16_t)((temp_raw * 2000UL) / AHT2X_SCALE) - 500;
    out->valid    = 1;

    return 1;
}

void StartTempTask(void *argument)
{
    TempData sample;

    (void)argument;

    s_data.valid = 0;
    s_data.temp_x10 = 0;
    s_data.hum_x10 = 0;

    AHT2X_Init();   ///< failure is reported through the data flag

    for (;;) {
        sample.valid = 0;

        if (!AHT2X_ReadData(&sample)) {
            sample.temp_x10 = 0;
            sample.hum_x10 = 0;
        }

        __disable_irq();
        s_data = sample;
        s_seq++;
        __enable_irq();

        osDelay(TEMP_PERIOD_MS);
    }
}

uint8_t TEMP_GetLatest(TempData *out)
{
    TempData local;
    uint8_t  new_seq;

    if (out == NULL) {
        return 0;
    }

    __disable_irq();
    local   = s_data;
    new_seq = s_seq;
    __enable_irq();

    *out = local;

    if (new_seq == s_readSeq) {
        return 0;  ///< nothing new
    }

    s_readSeq = new_seq;
    return 1;
}