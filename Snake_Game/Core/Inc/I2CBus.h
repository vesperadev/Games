#ifndef __I2C_BUS_H__
#define __I2C_BUS_H__

/// Shared I2C1 bus arbitration.
///
/// Every task that touches I2C1 (OLED, sensors, ...) must wrap its transactions
/// with I2CBus_Lock() / I2CBus_Unlock(). The underlying mutex (I2C1Mutex) is
/// created by CubeMX in freertos.c; its handle is known only to I2C.c.
///
/// Note: this header is NOT named I2C.h on purpose. On Windows the filesystem is
/// case insensitive, so I2C.h and the CubeMX generated i2c.h would be the same file.

void I2CBus_Lock(void);
void I2CBus_Unlock(void);

#endif // __I2C_BUS_H__
