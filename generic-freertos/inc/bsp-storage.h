/*
 * File: bsp-storage.h
 *
 * Purpose:
 *   The storage contract the BSP exposes to the FreeRTOS OS layer. The BSP
 *   owns the hardware setup; the OS layer (freertos/src/os-impl-filesys.c)
 *   mounts littlefs on whatever configuration this returns.
 *
 *   A storage provider is any object that defines OS_BSP_Storage_GetConfig(),
 *   built and linked into the executable (for the simulator, see
 *   test-fixtures/storage-posix-ram). The provider owns the geometry, buffers
 *   and device callbacks, so the BSP itself knows nothing about the device.
 */

#ifndef BSP_STORAGE_H
#define BSP_STORAGE_H

#include "lfs.h"

/*----------------------------------------------------------------
   OS_BSP_Storage_GetConfig

   Returns the littlefs configuration for the BSP's storage device. The
   pointer is valid for the life of the program and must not be modified.
   Read, prog and lookahead buffers are static, so littlefs does not allocate.

   The cache_size must not exceed OS_FREERTOS_LFS_FILE_CACHE_SIZE (see
   freertos/inc/os-impl-filesys.h), which is what each open file reserves.
 ------------------------------------------------------------------*/
const struct lfs_config *OS_BSP_Storage_GetConfig(void);

#endif /* BSP_STORAGE_H */
