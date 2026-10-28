#include "main.h"
#include <string.h>
#include "Score.h"

/// Last 1 KB page of the 64 KB flash: 0x0800FC00 - 0x0800FFFF.
/// The image is ~31 KB, so this page is never used by code.
#define SCORE_FLASH_ADDR   0x0800FC00UL
#define SCORE_MAGIC        0xA55A
#define SCORE_VERSION      1

/// Layout stored in flash. Keep the size even, HAL programs half words.
typedef struct {
    uint16_t magic;
    uint16_t version;
    uint16_t count;
    uint16_t score[SCORE_MAX];
    uint16_t checksum;
} ScoreBlob;

static ScoreBlob s_blob;

/// Simple sum over everything except the checksum field
static uint16_t blob_checksum(const ScoreBlob *b)
{
    const uint16_t *p = (const uint16_t *)b;
    uint16_t sum = 0;
    uint8_t  i;

    for (i = 0; i < (sizeof(ScoreBlob) - sizeof(uint16_t)) / 2; i++) {
        sum = (uint16_t)(sum + p[i]);
    }

    return sum;
}

/// Erase the page and program the blob
static uint8_t blob_save(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    const uint16_t *src = (const uint16_t *)&s_blob;
    uint8_t ok = 1;
    uint32_t i;

    s_blob.magic = SCORE_MAGIC;
    s_blob.version = SCORE_VERSION;
    s_blob.checksum = blob_checksum(&s_blob);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = SCORE_FLASH_ADDR;
    erase.NbPages = 1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        ok = 0;
    }

    for (i = 0; ok && i < sizeof(ScoreBlob) / 2; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              SCORE_FLASH_ADDR + i * 2, src[i]) != HAL_OK) {
            ok = 0;
        }
    }

    HAL_FLASH_Lock();

    return ok;
}

void SCORE_Load(void)
{
    memcpy(&s_blob, (const void *)SCORE_FLASH_ADDR, sizeof(s_blob));

    if (s_blob.magic != SCORE_MAGIC ||
        s_blob.version != SCORE_VERSION ||
        s_blob.count > SCORE_MAX ||
        s_blob.checksum != blob_checksum(&s_blob)) {
        memset(&s_blob, 0, sizeof(s_blob));
        s_blob.magic = SCORE_MAGIC;
        s_blob.version = SCORE_VERSION;
        s_blob.checksum = blob_checksum(&s_blob);
    }
}

uint8_t SCORE_Count(void)
{
    return (uint8_t)s_blob.count;
}

uint16_t SCORE_Get(uint8_t index)
{
    if (index >= SCORE_MAX) {
        return 0;
    }
    return s_blob.score[index];
}

uint8_t SCORE_Add(uint16_t score)
{
    uint8_t i;

    if (score == 0) {
        return 0;   ///< a zero score is not worth a flash write
    }

    /// find the insert position (table is sorted descending)
    for (i = 0; i < SCORE_MAX; i++) {
        if (i >= s_blob.count || score > s_blob.score[i]) {
            break;
        }
    }

    if (i >= SCORE_MAX) {
        return 0;   ///< worse than the 10th entry
    }

    /// shift the tail down
    for (uint8_t j = SCORE_MAX - 1; j > i; j--) {
        s_blob.score[j] = s_blob.score[j - 1];
    }
    s_blob.score[i] = score;

    if (s_blob.count < SCORE_MAX) {
        s_blob.count++;
    }

    blob_save();

    return 1;
}

void SCORE_Clear(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    blob_save();
}
