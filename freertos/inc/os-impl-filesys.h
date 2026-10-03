/*
 * File: os-impl-filesys.h
 *
 * Purpose:
 *   The littlefs volume shared by the FreeRTOS file and filesystem
 *   implementations. There is one volume, mounted from the configuration the
 *   BSP exposes (bsp-storage.h). Every littlefs call runs under one mutex.
 */

#ifndef OS_IMPL_FILESYS_H
#define OS_IMPL_FILESYS_H

#include "osconfig.h"
#include "osapi-common.h"

#include "lfs.h"

/*
 * Cache bytes each open file reserves for littlefs. The BSP's cache_size must
 * not exceed this, which OS_FileSysStartVolume_Impl() checks.
 */
#define OS_FREERTOS_LFS_FILE_CACHE_SIZE 512

/* The mounted volume. Only touch it between OS_FreeRTOS_LfsAcquire() and Release() */
extern lfs_t OS_impl_lfs;

/*----------------------------------------------------------------
   OS_FreeRTOS_LfsAcquire

   Takes the littlefs lock and checks the volume is mounted. Returns
   OS_SUCCESS with the lock held, or OS_ERROR with it released. The timeout
   for file I/O is not applied here: the lock is held for one littlefs call.
 ------------------------------------------------------------------*/
int32 OS_FreeRTOS_LfsAcquire(void);

/*----------------------------------------------------------------
   OS_FreeRTOS_LfsRelease

   Releases the lock taken by a successful OS_FreeRTOS_LfsAcquire().
 ------------------------------------------------------------------*/
void OS_FreeRTOS_LfsRelease(void);

/*----------------------------------------------------------------
   OS_FreeRTOS_LfsResult

   Maps a littlefs return code to OS_SUCCESS (rc >= 0) or OS_ERROR.
 ------------------------------------------------------------------*/
int32 OS_FreeRTOS_LfsResult(int rc);

#endif /* OS_IMPL_FILESYS_H */
