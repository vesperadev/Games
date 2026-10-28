#include "main.h"
#include "cmsis_os2.h"
#include "I2CBus.h"

/// I2C1 bus mutex, created by MX_FREERTOS_Init()
/// (CubeMX: FREERTOS -> Mutexes -> I2C1Mutex).
/// The handle is declared here and nowhere else, so the rest of the project
/// only ever sees I2C1Bus_Lock() / I2C1Bus_Unlock().
extern osMutexId_t I2C1MutexHandle;

void I2C1Bus_Lock(void)
{
    osMutexAcquire(I2C1MutexHandle, osWaitForever);
}

void I2C1Bus_Unlock(void)
{
    osMutexRelease(I2C1MutexHandle);
}