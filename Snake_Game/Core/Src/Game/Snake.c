#include "Snake.h"

static SnakeCell s_body[SNAKE_MAX_LEN];
static uint8_t   s_len;
static SnakeDir  s_dir;
static SnakeCell s_food;
static uint16_t  s_score;
static uint32_t  s_rng;
static uint8_t   s_turned;   ///< a turn was already accepted since the last move

/// Small LCG, good enough for food placement
static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (s_rng >> 16) & 0x7FFFu;
}

static uint8_t cell_used(uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < s_len; i++) {
        if (s_body[i].x == x && s_body[i].y == y) {
            return 1;
        }
    }
    return 0;
}

static void place_food(void)
{
    uint16_t tries = 0;

    do {
        s_food.x = (uint8_t)(rnd() % SNAKE_COLS);
        s_food.y = (uint8_t)(rnd() % SNAKE_ROWS);
    } while (cell_used(s_food.x, s_food.y) && ++tries < 500);
}

void SNAKE_Reset(uint32_t seed)
{
    s_rng = (seed != 0) ? seed : 1;
    s_len = 2;
    s_dir = (SnakeDir)(rnd() % 4);
    s_score = 0;
    s_turned = 0;

    s_body[0].x = SNAKE_COLS / 2;
    s_body[0].y = SNAKE_ROWS / 2;

    /// the tail sits behind the head, opposite the movement direction
    s_body[1] = s_body[0];
    switch (s_dir) {
    case DIR_UP:    s_body[1].y++; break;
    case DIR_DOWN:  s_body[1].y--; break;
    case DIR_LEFT:  s_body[1].x++; break;
    default:        s_body[1].x--; break;
    }

    place_food();
}

void SNAKE_Turn(int8_t steps)
{
    /// One 90 degree turn per move. Without this, several encoder steps arriving
    /// in the same second would stack up into 180/270 degrees, i.e. the snake
    /// appears to walk backwards.
    if (s_turned) {
        return;
    }

    if (steps > 0) {
        s_dir = (SnakeDir)((s_dir + 1) % 4);        ///< clockwise
        s_turned = 1;
    } else if (steps < 0) {
        s_dir = (SnakeDir)((s_dir + 3) % 4);        ///< counter-clockwise
        s_turned = 1;
    }
}

SnakeStepResult SNAKE_Step(void)
{
    SnakeCell head = s_body[0];
    uint8_t   eat;

    /// wall check
    switch (s_dir) {
    case DIR_UP:
        if (head.y == 0) return SNAKE_STEP_DEAD;
        head.y--;
        break;
    case DIR_DOWN:
        if (head.y + 1 >= SNAKE_ROWS) return SNAKE_STEP_DEAD;
        head.y++;
        break;
    case DIR_LEFT:
        if (head.x == 0) return SNAKE_STEP_DEAD;
        head.x--;
        break;
    default: /* DIR_RIGHT */
        if (head.x + 1 >= SNAKE_COLS) return SNAKE_STEP_DEAD;
        head.x++;
        break;
    }

    eat = (uint8_t)(head.x == s_food.x && head.y == s_food.y);

    /// Never step onto the neck. With a 2 segment snake the neck is also the tail
    /// and the generic self collision test below skips it, which would let the
    /// snake run backwards after a 180 degree turn.
    if (s_len > 1 && head.x == s_body[1].x && head.y == s_body[1].y) {
        return SNAKE_STEP_DEAD;
    }

    /// self collision; when not eating the tail cell is freed this step
    for (uint8_t i = 0; i < s_len - (eat ? 0 : 1); i++) {
        if (s_body[i].x == head.x && s_body[i].y == head.y) {
            return SNAKE_STEP_DEAD;
        }
    }

    /// grow first so the shift below keeps the extra segment
    if (eat && s_len < SNAKE_MAX_LEN) {
        s_body[s_len] = s_body[s_len - 1];
        s_len++;
    }

    for (uint8_t i = s_len - 1; i > 0; i--) {
        s_body[i] = s_body[i - 1];
    }
    s_body[0] = head;

    s_turned = 0;   ///< a new move cycle may accept one turn again

    if (eat) {
        s_score++;
        place_food();
        return SNAKE_STEP_ATE;
    }

    return SNAKE_STEP_MOVED;
}

uint8_t SNAKE_Length(void)
{
    return s_len;
}

const SnakeCell *SNAKE_Body(void)
{
    return s_body;
}

SnakeCell SNAKE_Food(void)
{
    return s_food;
}

uint16_t SNAKE_Score(void)
{
    return s_score;
}
