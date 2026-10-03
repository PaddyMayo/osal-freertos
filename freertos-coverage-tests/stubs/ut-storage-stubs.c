/*
 * File: ut-storage-stubs.c
 *
 * Purpose:
 *   RAM-backed littlefs volume standing in for the BSP's storage, so the
 *   white-box tests for os-impl-filesys.c and os-impl-file.c run real littlefs
 *   code. OS_BSP_Storage_GetConfig() is the BSP's real entry point; here it
 *   returns a configuration over this RAM instead of storage_hw.c.
 *
 *   Controls for failures and the cache size are in ../adaptors/inc/ut-adaptor-storage.h.
 */

#include <string.h>

#include "lfs.h"

#include "bsp-storage.h"
#include "os-impl-filesys.h"

#include "ut-adaptor-storage.h"

#define UT_STORAGE_READ_SIZE      16
#define UT_STORAGE_PROG_SIZE      16
#define UT_STORAGE_BLOCK_SIZE     512
#define UT_STORAGE_BLOCK_COUNT    64
#define UT_STORAGE_CACHE_SIZE     512
#define UT_STORAGE_LOOKAHEAD_SIZE 16
#define UT_STORAGE_BLOCK_CYCLES   500

int UT_Storage_FailReads;
int UT_Storage_FailProgs;

static uint8_t UT_Storage_Ram[UT_STORAGE_BLOCK_COUNT * UT_STORAGE_BLOCK_SIZE];

static uint8_t UT_Storage_ReadBuffer[UT_STORAGE_CACHE_SIZE];
static uint8_t UT_Storage_ProgBuffer[UT_STORAGE_CACHE_SIZE];
static uint8_t UT_Storage_LookaheadBuffer[UT_STORAGE_LOOKAHEAD_SIZE];

static int UT_Storage_Read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;

    if (UT_Storage_FailReads != 0)
    {
        return UT_Storage_FailReads;
    }

    memcpy(buffer, &UT_Storage_Ram[(block * UT_STORAGE_BLOCK_SIZE) + off], size);
    return LFS_ERR_OK;
}

static int
UT_Storage_Prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    (void)c;

    if (UT_Storage_FailProgs != 0)
    {
        return UT_Storage_FailProgs;
    }

    memcpy(&UT_Storage_Ram[(block * UT_STORAGE_BLOCK_SIZE) + off], buffer, size);
    return LFS_ERR_OK;
}

static int UT_Storage_Erase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;

    memset(&UT_Storage_Ram[block * UT_STORAGE_BLOCK_SIZE], 0xFF, UT_STORAGE_BLOCK_SIZE);
    return LFS_ERR_OK;
}

static int UT_Storage_Sync(const struct lfs_config *c)
{
    (void)c;

    return LFS_ERR_OK;
}

static struct lfs_config UT_Storage_Config = {
    .read             = UT_Storage_Read,
    .prog             = UT_Storage_Prog,
    .erase            = UT_Storage_Erase,
    .sync             = UT_Storage_Sync,
    .read_size        = UT_STORAGE_READ_SIZE,
    .prog_size        = UT_STORAGE_PROG_SIZE,
    .block_size       = UT_STORAGE_BLOCK_SIZE,
    .block_count      = UT_STORAGE_BLOCK_COUNT,
    .block_cycles     = UT_STORAGE_BLOCK_CYCLES,
    .cache_size       = UT_STORAGE_CACHE_SIZE,
    .lookahead_size   = UT_STORAGE_LOOKAHEAD_SIZE,
    .read_buffer      = UT_Storage_ReadBuffer,
    .prog_buffer      = UT_Storage_ProgBuffer,
    .lookahead_buffer = UT_Storage_LookaheadBuffer,
};

const struct lfs_config *OS_BSP_Storage_GetConfig(void)
{
    return &UT_Storage_Config;
}

void UT_Storage_Reset(void)
{
    memset(UT_Storage_Ram, 0, sizeof(UT_Storage_Ram));
    UT_Storage_FailReads         = 0;
    UT_Storage_FailProgs         = 0;
    UT_Storage_Config.cache_size = UT_STORAGE_CACHE_SIZE;
}

void UT_Storage_SetCacheSize(lfs_size_t size)
{
    UT_Storage_Config.cache_size = size;
}
