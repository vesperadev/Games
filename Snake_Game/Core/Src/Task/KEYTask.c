#include "main.h"
#include <stdint.h>
#include "KEYTask.h"

/// Tunable parameters (unit: ms)
#define KEY_SCAN_MS        10    ///< scan period
#define KEY_DEBOUNCE_MS    20    ///< debounce time
#define KEY_SHORT_MIN_MS   30    ///< minimum press time to count as a click (shorter = bounce)
#define KEY_LONG_MS        1000  ///< long press threshold
#define KEY_DOUBLE_GAP_MS  300   ///< maximum gap between the two clicks of a double click

/// Per-key configuration and runtime state
typedef struct {
    GPIO_TypeDef      *port;   ///< GPIO port
    uint16_t           pin;    ///< GPIO pin
    osMessageQueueId_t queue;  ///< event queue

    /// --- configuration ---
    uint8_t  enableDouble;     ///< 1 = double click enabled (a click is reported KEY_DOUBLE_GAP_MS later)
                               ///< 0 = click / long press only, a click is reported right on release

    /// --- runtime state ---
    uint8_t  raw;              ///< last raw sample, 1 = pressed
    uint32_t rawTick;          ///< tick of the last change of raw
    uint8_t  stable;           ///< debounced state, 1 = pressed

    uint32_t pressTick;        ///< tick when the press became stable
    uint8_t  longSent;         ///< long press already reported for this press

    uint8_t  clickPending;     ///< a click was released and waits for a possible second click
    uint32_t clickTick;        ///< release tick of that click
    uint8_t  suppressClick;    ///< this press was consumed as the 2nd click, no click on release
} KeyState;

/// Key table.
/// enableDouble = 0: click / long press, reported immediately on release (no 300 ms wait)
/// enableDouble = 1: double click supported, a click is reported only after the double click window
/// NOTE: do not map both "single click" and "double click" to the same key, otherwise the
///       waiting time is unavoidable.
static KeyState s_keys[] = {
    { .port = KEY1_GPIO_Port, .pin = KEY1_Pin, .enableDouble = 0 },  ///< KEY1: PB12
    { .port = KEY2_GPIO_Port, .pin = KEY2_Pin, .enableDouble = 0 },  ///< KEY2: PB13
};

#define KEY_NUM (sizeof(s_keys) / sizeof(s_keys[0]))

/// Push one event into the key queue
static void KEY_Send(KeyState *k, KeyEvent evt)
{
    uint16_t msg = (uint16_t)evt;

    /// drop the event if the queue is full, never block the scan loop
    osMessageQueuePut(k->queue, &msg, 0, 0);
}

/// Sample one key, apply debounce and report edges
static void KEY_ScanEdge(KeyState *k, uint32_t now)
{
    uint8_t pressed = (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_RESET) ? 1 : 0;

    /// raw level changed, restart the debounce timer
    if (pressed != k->raw) {
        k->raw = pressed;
        k->rawTick = now;
        return;
    }

    /// not stable long enough, or the debounced state did not change
    if ((now - k->rawTick) < KEY_DEBOUNCE_MS || pressed == k->stable) {
        return;
    }

    k->stable = pressed;

    if (pressed) {
        /// stable press
        k->pressTick = now;
        k->longSent = 0;

        if (k->enableDouble && k->clickPending) {
            /// the previous click is still inside the double click window
            k->clickPending = 0;
            k->suppressClick = 1;
            KEY_Send(k, KEY_EVT_DOUBLE);
        }
    } else {
        /// stable release
        uint32_t held = now - k->pressTick;

        if (!k->longSent && !k->suppressClick && held >= KEY_SHORT_MIN_MS) {
            if (k->enableDouble) {
                /// hold the event until the double click window expires
                k->clickPending = 1;
                k->clickTick = now;
            } else {
                /// double click disabled: report the click immediately
                KEY_Send(k, KEY_EVT_SHORT);
            }
        }

        k->longSent = 0;
        k->suppressClick = 0;
    }
}

/// Timeout handling: report a long press while held, and a click when the double click window expires
static void KEY_PollTimeout(KeyState *k, uint32_t now)
{
    if (k->stable && !k->longSent && (now - k->pressTick) >= KEY_LONG_MS) {
        k->longSent = 1;
        k->clickPending = 0;
        KEY_Send(k, KEY_EVT_LONG);
    }

    if (k->clickPending && (now - k->clickTick) >= KEY_DOUBLE_GAP_MS) {
        k->clickPending = 0;
        KEY_Send(k, KEY_EVT_SHORT);
    }
}

void StartKEYTask(void *argument){
    (void)argument;

    /// the queue handles are created by MX_FREERTOS_Init() before this task starts
    s_keys[0].queue = KEY1QueueHandle;
    s_keys[1].queue = KEY2QueueHandle;

    for (;;) {
        uint32_t now = osKernelGetTickCount();

        for (uint32_t i = 0; i < KEY_NUM; i++) {
            KEY_ScanEdge(&s_keys[i], now);
            KEY_PollTimeout(&s_keys[i], now);
        }

        osDelay(KEY_SCAN_MS);
    }
}