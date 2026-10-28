#ifndef __SNAKE_H__
#define __SNAKE_H__

#include <stdint.h>

/// One cell is SNAKE_CELL x SNAKE_CELL pixels -> 32 x 16 cells on a 128x64 panel
#define SNAKE_CELL      4
#define SNAKE_COLS      (128 / SNAKE_CELL)
#define SNAKE_ROWS      (64 / SNAKE_CELL)
#define SNAKE_MAX_LEN   (SNAKE_COLS * SNAKE_ROWS)

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
} SnakeDir;

typedef struct {
    uint8_t x;
    uint8_t y;
} SnakeCell;

typedef enum {
    SNAKE_STEP_MOVED = 0,
    SNAKE_STEP_ATE,
    SNAKE_STEP_DEAD,
} SnakeStepResult;

/// Start a new game: length 2, head in the middle, random direction and food
void SNAKE_Reset(uint32_t seed);

/// Rotate the head. 1 = clockwise (right turn), -1 = counter-clockwise.
void SNAKE_Turn(int8_t steps);

/// Advance one cell
SnakeStepResult SNAKE_Step(void);

uint8_t SNAKE_Length(void);
const SnakeCell *SNAKE_Body(void);   ///< [0] is the head
SnakeCell SNAKE_Food(void);
uint16_t SNAKE_Score(void);

#endif // __SNAKE_H__
