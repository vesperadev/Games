#ifndef __ENCODER_TASK_H__
#define __ENCODER_TASK_H__

#include <stdint.h>

/// Task entry, bound by MX_FREERTOS_Init()
void StartEncoderTask(void *argument);

/// Steps (detents) turned since the last call.
/// Positive = clockwise, negative = counter-clockwise.
/// The value is consumed by the call.
int8_t ENCODER_TakeSteps(void);

#endif // __ENCODER_TASK_H__
