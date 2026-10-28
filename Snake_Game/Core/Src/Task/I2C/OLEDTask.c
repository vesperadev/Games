#include "main.h"
#include "oled.h"
#include "font.h"
#include "I2CBus.h"
#include "KEYTask.h"
#include "TempTask.h"
#include "EncoderTask.h"
#include "Snake.h"
#include "Score.h"
#include "cmsis_os.h"

/// Screens shown by the UI task
typedef enum {
    SCREEN_MENU = 0,
    SCREEN_SENSOR,
    SCREEN_SNAKE_MENU,
    SCREEN_SNAKE_PLAY,
    SCREEN_SNAKE_OVER,
} Screen;

/// Main menu layout
#define MENU_COUNT      2
#define MENU_TEXT_X     10                   ///< text left margin, leaves room for the arrow
#define MENU_ROW_Y(i)   (20 + (i) * 18)      ///< row spacing: 18 px per line

/// High score list layout
#define SCORE_ROWS      4                    ///< entries visible at once
#define SCORE_ROW_Y(i)  (30 + (i) * 7)       ///< 7 px per row (afont8x6 is 8 px high)

/// Snake timing
#define SNAKE_TICK_MS   1000                 ///< one move per second
#define OVER_SHOW_MS    2000                 ///< how long Game Over stays on screen

static Screen   s_screen = SCREEN_MENU;  ///< screen currently shown
static uint8_t  s_selected = 0;          ///< selected main menu item
static uint8_t  s_scoreSel = 0;          ///< selected high score entry
static uint8_t  s_scoreTop = 0;          ///< first visible high score entry
static uint16_t s_score = 0;             ///< score of the current game
static uint32_t s_nextMove = 0;          ///< tick of the next snake move
static uint32_t s_overTick = 0;          ///< tick when Game Over was shown

/// Main menu items.
/// The zh16x16 font does not contain the needed CJK glyphs, so English is used for now.
static const char *const kMenuText[MENU_COUNT] = {
    "Snake Game",     ///< item 0
    "Temp/Humidity",  ///< item 1
};

/// Push the frame buffer to the panel.
/// I2C1 is shared with the sensor task, so the transfer must hold the bus mutex.
static void OLED_Push(void)
{
    I2C1Bus_Lock();
    OLED_ShowFrame();
    I2C1Bus_Unlock();
}

/// Write a decimal number into buf (at least 6 bytes) and return its length
static uint8_t u16_str(uint16_t v, char *buf)
{
    char    tmp[6];
    uint8_t n = 0;
    uint8_t i = 0;

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (v > 0 && n < 5) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (n > 0) {
        buf[i++] = tmp[--n];
    }
    buf[i] = '\0';

    return i;
}

/// Build "<prefix><v/10>.<v%10><suffix>" into buf (at least 20 bytes).
/// Avoids pulling printf into the image just to show one decimal.
static void fmt_x10(char *buf, const char *prefix, int32_t v10, const char *suffix)
{
    char     digits[8];
    uint8_t  n = 0;
    uint32_t v;
    uint32_t ip;

    while (*prefix) {
        *buf++ = *prefix++;
    }

    if (v10 < 0) {
        *buf++ = '-';
        v = (uint32_t)(-v10);
    } else {
        v = (uint32_t)v10;
    }

    ip = v / 10;
    if (ip == 0) {
        digits[n++] = '0';
    }
    while (ip > 0) {
        digits[n++] = (char)('0' + (ip % 10));
        ip /= 10;
    }
    while (n > 0) {
        *buf++ = digits[--n];
    }

    *buf++ = '.';
    *buf++ = (char)('0' + (v % 10));

    while (*suffix) {
        *buf++ = *suffix++;
    }
    *buf = '\0';
}

/// Main menu screen: title, separator and items, arrow marks the selection
static void MENU_Draw(void)
{
    OLED_NewFrame();

    OLED_PrintASCIIString(0, 0, "MENU", &afont16x8, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 17, 127, 17, OLED_COLOR_NORMAL);

    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        if (i == s_selected) {
            OLED_PrintASCIIString(0, MENU_ROW_Y(i), ">", &afont16x8, OLED_COLOR_NORMAL);
        }
        OLED_PrintASCIIString(MENU_TEXT_X, MENU_ROW_Y(i), kMenuText[i],
                              &afont16x8, OLED_COLOR_NORMAL);
    }

    OLED_Push();
}

/// Temperature / humidity screen
static void SENSOR_Draw(const TempData *d)
{
    char line[20];

    OLED_NewFrame();

    OLED_PrintASCIIString(0, 0, "SENSOR", &afont16x8, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 17, 127, 17, OLED_COLOR_NORMAL);

    if (d->valid) {
        fmt_x10(line, "T: ", d->temp_x10, " C");
        OLED_PrintASCIIString(0, 20, line, &afont16x8, OLED_COLOR_NORMAL);

        fmt_x10(line, "H: ", d->hum_x10, " %");
        OLED_PrintASCIIString(0, 38, line, &afont16x8, OLED_COLOR_NORMAL);
    } else {
        /// sensor not answering: keep the layout, show placeholders
        OLED_PrintASCIIString(0, 20, "T: --", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(0, 38, "H: --", &afont16x8, OLED_COLOR_NORMAL);
    }

    OLED_Push();
}

/// Snake game screen: start hint + scrollable top-10 list
static void SNAKE_MENU_Draw(void)
{
    char    line[16];
    uint8_t count = SCORE_Count();

    /// keep the cursor inside the visible window
    if (s_scoreSel < s_scoreTop) {
        s_scoreTop = s_scoreSel;
    } else if (s_scoreSel >= (uint8_t)(s_scoreTop + SCORE_ROWS)) {
        s_scoreTop = (uint8_t)(s_scoreSel - SCORE_ROWS + 1);
    }

    OLED_NewFrame();

    OLED_PrintASCIIString(0, 0, "SNAKE GAME", &afont16x8, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 17, 127, 17, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(0, 19, "K2:START", &afont8x6, OLED_COLOR_NORMAL);

    if (count == 0) {
        OLED_PrintASCIIString(0, 30, "NO RECORD", &afont8x6, OLED_COLOR_NORMAL);
    } else {
        for (uint8_t i = 0; i < SCORE_ROWS; i++) {
            uint8_t idx = (uint8_t)(s_scoreTop + i);
            uint8_t p = 0;

            if (idx >= count) {
                break;
            }

            line[p++] = (idx == s_scoreSel) ? '>' : ' ';
            line[p++] = ' ';
            p = (uint8_t)(p + u16_str((uint16_t)(idx + 1), &line[p]));
            line[p++] = ':';
            p = (uint8_t)(p + u16_str(SCORE_Get(idx), &line[p]));

            OLED_PrintASCIIString(0, SCORE_ROW_Y(i), line, &afont8x6, OLED_COLOR_NORMAL);
        }
    }

    OLED_Push();
}

/// Play field: 32 x 16 cells of 4 x 4 pixels, food as a dot
static void SNAKE_PLAY_Draw(void)
{
    const SnakeCell *body = SNAKE_Body();
    uint8_t          len = SNAKE_Length();
    SnakeCell        food = SNAKE_Food();

    OLED_NewFrame();

    OLED_DrawFilledCircle((uint8_t)(food.x * SNAKE_CELL + SNAKE_CELL / 2),
                          (uint8_t)(food.y * SNAKE_CELL + SNAKE_CELL / 2),
                          2, OLED_COLOR_NORMAL);

    for (uint8_t i = 1; i < len; i++) {
        OLED_DrawFilledRectangle((uint8_t)(body[i].x * SNAKE_CELL),
                                 (uint8_t)(body[i].y * SNAKE_CELL),
                                 SNAKE_CELL, SNAKE_CELL, OLED_COLOR_NORMAL);
    }

    /// head: filled square with a hole in the middle, so the leading cell and
    /// the direction of travel are obvious on screen
    OLED_DrawFilledRectangle((uint8_t)(body[0].x * SNAKE_CELL),
                             (uint8_t)(body[0].y * SNAKE_CELL),
                             SNAKE_CELL, SNAKE_CELL, OLED_COLOR_NORMAL);
    OLED_DrawFilledRectangle((uint8_t)(body[0].x * SNAKE_CELL + 1),
                             (uint8_t)(body[0].y * SNAKE_CELL + 1),
                             2, 2, OLED_COLOR_REVERSED);

    OLED_Push();
}

/// Game over screen
static void SNAKE_OVER_Draw(void)
{
    char        line[16];
    uint8_t     p = 0;
    const char *prefix = "SCORE: ";

    while (*prefix) {
        line[p++] = *prefix++;
    }
    (void)u16_str(s_score, &line[p]);

    OLED_NewFrame();
    OLED_PrintASCIIString(28, 12, "GAME OVER", &afont16x8, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(28, 36, line, &afont16x8, OLED_COLOR_NORMAL);
    OLED_Push();
}

/// Drain both key queues and update the screen state
/// @return 1 if a redraw is needed
static uint8_t UI_HandleKeys(void)
{
    uint16_t evt;
    uint8_t  changed = 0;

    while (osMessageQueueGet(KEY1QueueHandle, &evt, NULL, 0) == osOK) {
        switch (s_screen) {
        case SCREEN_MENU:
            if (evt == KEY_EVT_SHORT) {
                s_selected = (uint8_t)((s_selected + 1) % MENU_COUNT);
                changed = 1;
            } else if (evt == KEY_EVT_LONG) {
                if (s_selected == 1) {
                    s_screen = SCREEN_SENSOR;
                } else {
                    s_screen = SCREEN_SNAKE_MENU;
                    s_scoreSel = 0;
                    s_scoreTop = 0;
                }
                changed = 1;
            }
            break;

        case SCREEN_SENSOR:
            if (evt == KEY_EVT_LONG) {
                s_screen = SCREEN_MENU;
                changed = 1;
            }
            break;

        case SCREEN_SNAKE_MENU: {
            /// short = next entry, long = previous entry
            uint8_t n = SCORE_Count();

            if (n == 0) {
                n = 1;
            }
            if (evt == KEY_EVT_SHORT) {
                s_scoreSel = (uint8_t)((s_scoreSel + 1) % n);
                changed = 1;
            } else if (evt == KEY_EVT_LONG) {
                s_scoreSel = (uint8_t)((s_scoreSel + n - 1) % n);
                changed = 1;
            }
            break;
        }

        case SCREEN_SNAKE_PLAY:
            if (evt == KEY_EVT_LONG) {   ///< abort the game
                s_screen = SCREEN_SNAKE_MENU;
                changed = 1;
            }
            break;

        default:
            break;
        }
    }

    while (osMessageQueueGet(KEY2QueueHandle, &evt, NULL, 0) == osOK) {
        switch (s_screen) {
        case SCREEN_MENU:
            if (evt == KEY_EVT_SHORT) {
                s_selected = (uint8_t)((s_selected + MENU_COUNT - 1) % MENU_COUNT);
                changed = 1;
            }
            break;

        case SCREEN_SENSOR:
            if (evt == KEY_EVT_SHORT || evt == KEY_EVT_LONG) {
                s_screen = SCREEN_MENU;
                changed = 1;
            }
            break;

        case SCREEN_SNAKE_MENU:
            if (evt == KEY_EVT_SHORT) {   ///< start a new game
                uint32_t now = osKernelGetTickCount();

                (void)ENCODER_TakeSteps();   ///< drop rotation done while in the menu

                SNAKE_Reset(now);
                s_score = 0;
                s_nextMove = now + SNAKE_TICK_MS;
                s_screen = SCREEN_SNAKE_PLAY;
                changed = 1;
            }
            break;

        default:
            break;
        }
    }

    return changed;
}

void StartOLEDTask(void *argument)
{
    TempData data;

    (void)argument;

    SCORE_Load();

    I2C1Bus_Lock();
    OLED_Init();   ///< clears the screen internally
    I2C1Bus_Unlock();

    MENU_Draw();   ///< first frame

    for (;;) {
        uint8_t  redraw    = UI_HandleKeys();
        uint8_t  newSample = TEMP_GetLatest(&data);
        uint32_t now       = osKernelGetTickCount();

        switch (s_screen) {
        case SCREEN_MENU:
            if (redraw) {
                MENU_Draw();
            }
            break;

        case SCREEN_SENSOR:
            if (redraw || newSample) {
                SENSOR_Draw(&data);   ///< refresh every 2 s or when the screen changes
            }
            break;

        case SCREEN_SNAKE_MENU:
            if (redraw) {
                SNAKE_MENU_Draw();
            }
            break;

        case SCREEN_SNAKE_PLAY: {
            uint8_t needDraw = 0;

            if ((int32_t)(now - s_nextMove) >= 0) {
                SnakeStepResult r;
                int8_t          turn;

                /// Read the encoder only once per move. What matters is the net
                /// rotation since the previous move, not how many detents passed.
                turn = ENCODER_TakeSteps();
                if (turn > 0) {
                    SNAKE_Turn(1);        ///< clockwise
                } else if (turn < 0) {
                    SNAKE_Turn(-1);       ///< counter-clockwise
                }

                r = SNAKE_Step();

                s_nextMove = now + SNAKE_TICK_MS;

                if (r == SNAKE_STEP_DEAD) {
                    s_score = SNAKE_Score();
                    (void)SCORE_Add(s_score);
                    s_overTick = now;
                    s_screen = SCREEN_SNAKE_OVER;
                    SNAKE_OVER_Draw();
                } else {
                    if (r == SNAKE_STEP_ATE) {
                        s_score = SNAKE_Score();
                    }
                    needDraw = 1;
                }
            }

            if (s_screen == SCREEN_SNAKE_PLAY && (redraw || needDraw)) {
                SNAKE_PLAY_Draw();
            }
            break;
        }

        case SCREEN_SNAKE_OVER:
            if ((now - s_overTick) >= OVER_SHOW_MS) {
                s_screen = SCREEN_SNAKE_MENU;
                SNAKE_MENU_Draw();
            }
            break;

        default:
            break;
        }

        osDelay(10);
    }
}