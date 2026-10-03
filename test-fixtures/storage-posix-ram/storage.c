/*
 * File: storage.c
 *
 * Purpose:
 *   Test fixture: the storage a POSIX simulator build of the FreeRTOS OSAL
 *   links against. It implements OS_BSP_Storage_GetConfig() (see
 *   generic-freertos/inc/bsp-storage.h) over a RAM array, so nothing persists
 *   between runs. A vendor supplies the same function over its own flash.
 *
 *   Erase sets a block to 0xFF, as flash does, so littlefs sees the
 *   erase-before-program behaviour it relies on.
 */

#include <stdint.h>
#include <string.h>

#include "lfs.h"

#include "bsp-storage.h"

#define RAM_READ_SIZE      16
#define RAM_PROG_SIZE      16
#define RAM_BLOCK_SIZE     4096
#define RAM_BLOCK_COUNT    256
#define RAM_CACHE_SIZE     512
#define RAM_LOOKAHEAD_SIZE 16
#define RAM_BLOCK_CYCLES   500

static uint8_t RamBytes[RAM_BLOCK_COUNT * RAM_BLOCK_SIZE];

static uint8_t RamReadBuffer[RAM_CACHE_SIZE];
static uint8_t RamProgBuffer[RAM_CACHE_SIZE];
static uint8_t RamLookaheadBuffer[RAM_LOOKAHEAD_SIZE];

static int RamRead(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;

    memcpy(buffer, &RamBytes[(block * RAM_BLOCK_SIZE) + off], size);
    return LFS_ERR_OK;
}

static int RamProg(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    (void)c;

    memcpy(&RamBytes[(block * RAM_BLOCK_SIZE) + off], buffer, size);
    return LFS_ERR_OK;
}

static int RamErase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;

    memset(&RamBytes[block * RAM_BLOCK_SIZE], 0xFF, RAM_BLOCK_SIZE);
    return LFS_ERR_OK;
}

static int RamSync(const struct lfs_config *c)
{
    (void)c;

    return LFS_ERR_OK;
}

static const struct lfs_config RamConfig = {
    .read             = RamRead,
    .prog             = RamProg,
    .erase            = RamErase,
    .sync             = RamSync,
    .read_size        = RAM_READ_SIZE,
    .prog_size        = RAM_PROG_SIZE,
    .block_size       = RAM_BLOCK_SIZE,
    .block_count      = RAM_BLOCK_COUNT,
    .block_cycles     = RAM_BLOCK_CYCLES,
    .cache_size       = RAM_CACHE_SIZE,
    .lookahead_size   = RAM_LOOKAHEAD_SIZE,
    .read_buffer      = RamReadBuffer,
    .prog_buffer      = RamProgBuffer,
    .lookahead_buffer = RamLookaheadBuffer,
};

const struct lfs_config *OS_BSP_Storage_GetConfig(void)
{
    return &RamConfig;
}
