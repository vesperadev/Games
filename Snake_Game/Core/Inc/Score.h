#ifndef __SCORE_H__
#define __SCORE_H__

#include <stdint.h>

#define SCORE_MAX  10   ///< number of records kept

/// Load the table from the last flash page (falls back to empty on bad data)
void SCORE_Load(void);

/// @return number of valid records, 0..SCORE_MAX
uint8_t SCORE_Count(void);

/// @return score at @p index, 0 when out of range
uint16_t SCORE_Get(uint8_t index);

/// Insert a score, keep the table sorted descending and persist it.
/// @return 1 if the table changed (and flash was written), 0 otherwise
uint8_t SCORE_Add(uint16_t score);

/// Erase the table (also wipes the flash page)
void SCORE_Clear(void);

#endif // __SCORE_H__
