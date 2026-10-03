/**
 * \file
 *
 * Purpose: OSAL volume implementation on littlefs.
 *
 *          One littlefs volume is mounted from the configuration the BSP exposes
 *          through OS_BSP_Storage_GetConfig(). littlefs is not thread-safe, so
 *          every call runs under one mutex. The file operations in os-impl-file.c
 *          take the same lock through OS_FreeRTOS_LfsAcquire()/Release().
 *
 *          Mounting formats the volume only when littlefs reports it corrupt. Any
 *          other mount failure is returned unchanged so no data is erased.
 */

#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "os-shared-filesys.h"
#include "os-shared-idmap.h"
#include "bsp-storage.h"

#include "os-impl-filesys.h"

lfs_t OS_impl_lfs;

static bool              OS_impl_lfs_mounted;
static StaticSemaphore_t OS_impl_lfs_mutex_buffer;
static SemaphoreHandle_t OS_impl_lfs_mutex;

/*----------------------------------------------------------------
 *
 *  Purpose: Maps a littlefs return code to an OSAL status
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_LfsResult(int rc)
{
    return (rc < LFS_ERR_OK) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Takes the lock and checks the volume is mounted.
 *           See header for details
 *
 *-----------------------------------------------------------------*/
int32 OS_FreeRTOS_LfsAcquire(void)
{
    if (OS_impl_lfs_mutex == NULL)
    {
        return OS_ERROR;
    }

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);

    if (!OS_impl_lfs_mounted)
    {
        xSemaphoreGive(OS_impl_lfs_mutex);
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Releases the lock taken by OS_FreeRTOS_LfsAcquire()
 *
 *-----------------------------------------------------------------*/
void OS_FreeRTOS_LfsRelease(void)
{
    xSemaphoreGive(OS_impl_lfs_mutex);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Mounts the volume, formatting it only if it is corrupt.
 *           Must be called with the lock held.
 *
 *-----------------------------------------------------------------*/
static int32 OS_FreeRTOS_LfsMountLocked(void)
{
    const struct lfs_config *cfg = OS_BSP_Storage_GetConfig();
    int                      rc;

    if (OS_impl_lfs_mounted)
    {
        return OS_SUCCESS;
    }

    rc = lfs_mount(&OS_impl_lfs, cfg);
    if (rc == LFS_ERR_CORRUPT)
    {
        rc = lfs_format(&OS_impl_lfs, cfg);
        if (rc == LFS_ERR_OK)
        {
            rc = lfs_mount(&OS_impl_lfs, cfg);
        }
    }

    OS_impl_lfs_mounted = (rc == LFS_ERR_OK);

    return OS_FreeRTOS_LfsResult(rc);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Unmounts the volume if it is mounted.
 *           Must be called with the lock held.
 *
 *-----------------------------------------------------------------*/
static int32 OS_FreeRTOS_LfsUnmountLocked(void)
{
    int rc;

    if (!OS_impl_lfs_mounted)
    {
        return OS_SUCCESS;
    }

    rc                  = lfs_unmount(&OS_impl_lfs);
    OS_impl_lfs_mounted = false;

    return OS_FreeRTOS_LfsResult(rc);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Block visitor for the consistency check. It does nothing,
 *           because lfs_fs_traverse() reports corruption on its own.
 *
 *-----------------------------------------------------------------*/
static int OS_FreeRTOS_LfsVisitBlock(void *data, lfs_block_t block)
{
    (void)data;
    (void)block;

    return LFS_ERR_OK;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    const struct lfs_config      *cfg = OS_BSP_Storage_GetConfig();
    OS_filesys_internal_record_t *filesys;

    if (cfg->cache_size > OS_FREERTOS_LFS_FILE_CACHE_SIZE)
    {
        return OS_ERROR;
    }

    if (OS_impl_lfs_mutex == NULL)
    {
        OS_impl_lfs_mutex = xSemaphoreCreateMutexStatic(&OS_impl_lfs_mutex_buffer);
    }

    filesys            = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    filesys->blocksize = cfg->block_size;
    filesys->numblocks = cfg->block_count;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    int32 status;

    (void)token;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);
    status = OS_FreeRTOS_LfsUnmountLocked();
    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    int32 status;
    int   rc;

    (void)token;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);

    /* lfs_unmount() only releases memory and cannot fail, so its status is not checked */
    (void)OS_FreeRTOS_LfsUnmountLocked();
    rc     = lfs_format(&OS_impl_lfs, OS_BSP_Storage_GetConfig());
    status = OS_FreeRTOS_LfsResult(rc);

    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *  littlefs has no repair pass. The check walks every block, so it reports
 *  corruption but cannot fix it, and "repair" is ignored.
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysCheckVolume_Impl(const OS_object_token_t *token, bool repair)
{
    int32 status;

    (void)token;
    (void)repair;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);

    if (OS_impl_lfs_mounted)
    {
        status = OS_FreeRTOS_LfsResult(lfs_fs_traverse(&OS_impl_lfs, OS_FreeRTOS_LfsVisitBlock, NULL));
    }
    else
    {
        status = OS_ERROR;
    }

    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysStatVolume_Impl(const OS_object_token_t *token, OS_statvfs_t *result)
{
    const struct lfs_config *cfg = OS_BSP_Storage_GetConfig();
    int32                    status;

    (void)token;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);

    if (OS_impl_lfs_mounted)
    {
        lfs_ssize_t used = lfs_fs_size(&OS_impl_lfs);

        if (used >= 0)
        {
            result->block_size   = cfg->block_size;
            result->total_blocks = cfg->block_count;
            result->blocks_free  = cfg->block_count - used;
            status               = OS_SUCCESS;
        }
        else
        {
            status = OS_ERROR;
        }
    }
    else
    {
        status = OS_ERROR;
    }

    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    int32 status;

    (void)token;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);
    status = OS_FreeRTOS_LfsMountLocked();
    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    int32 status;

    (void)token;

    xSemaphoreTake(OS_impl_lfs_mutex, portMAX_DELAY);
    status = OS_FreeRTOS_LfsUnmountLocked();
    xSemaphoreGive(OS_impl_lfs_mutex);

    return status;
}
