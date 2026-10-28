#ifndef __TEMP_TASK_H__
#define __TEMP_TASK_H__

#include <stdint.h>

/// Latest measurement, scaled by 10 (e.g. 253 means 25.3)
typedef struct {
    int16_t temp_x10;   ///< temperature in 0.1 degC
    int16_t hum_x10;    ///< relative humidity in 0.1 %
    uint8_t valid;      ///< 1 = last measurement succeeded
} TempData;

/// Task entry, bound by MX_FREERTOS_Init()
void StartTempTask(void *argument);

/// Copy the latest sample into @p out.
/// @return 1 if a new sample arrived since the previous call, 0 otherwise
uint8_t TEMP_GetLatest(TempData *out);

#endif // __TEMP_TASK_H__
