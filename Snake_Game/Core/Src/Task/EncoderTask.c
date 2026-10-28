#include "main.h"
#include "tim.h"
#include "cmsis_os2.h"
#include "EncoderTask.h"

/// TIM1 runs in encoder mode TI12, so one mechanical detent gives 4 counts.
#define ENCODER_COUNTS_PER_STEP  4
#define ENCODER_SCAN_MS          100
#define ENCODER_MAX_STEPS        8    ///< clamp a burst so the consumer never sees huge jumps

/// 1 = flip the rotation sense. Set to 0 if clockwise starts to turn left.
#define ENCODER_INVERT           1

static volatile int32_t s_counts;   ///< raw counts not yet converted into steps

void StartEncoderTask(void *argument)
{
    uint16_t last;

    (void)argument;

    /// CubeMX only calls MX_TIM1_Init(), the encoder itself must be started here
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    last = 0;

    for (;;) {
        uint16_t now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
        /// 16-bit signed difference is wrap safe
        int16_t delta = (int16_t)(now - last);

        last = now;

        if (delta != 0) {
            __disable_irq();
            s_counts += delta;
            __enable_irq();
        }

        osDelay(ENCODER_SCAN_MS);
    }
}

int8_t ENCODER_TakeSteps(void)
{
    int32_t counts;
    int32_t steps;

    __disable_irq();
    counts = s_counts;
    s_counts = 0;
    __enable_irq();

    steps = counts / ENCODER_COUNTS_PER_STEP;

#if ENCODER_INVERT
    steps = -steps;   ///< swap clockwise / counter-clockwise
#endif

    if (steps > ENCODER_MAX_STEPS) {
        steps = ENCODER_MAX_STEPS;
    } else if (steps < -ENCODER_MAX_STEPS) {
        steps = -ENCODER_MAX_STEPS;
    }

    return (int8_t)steps;
}