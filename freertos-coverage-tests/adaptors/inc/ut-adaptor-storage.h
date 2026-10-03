/*
 * File: ut-adaptor-storage.h
 *
 * Purpose:
 *   Test controls for the RAM-backed littlefs volume in ../stubs/ut-storage-stubs.c.
 *   Lets a test reset the device, shrink or grow the cache the BSP reports,
 *   and make reads, programs or erases fail so littlefs error paths run.
 */

#ifndef UT_ADAPTOR_STORAGE_H
#define UT_ADAPTOR_STORAGE_H

#include "lfs.h"

/* Clears the RAM to erased-looking zeros, the default cache size, and all failure injection */
void UT_Storage_Reset(void);

/* Sets the cache size the configuration reports, in bytes */
void UT_Storage_SetCacheSize(lfs_size_t size);

/* Failure injection: when non-zero, the matching device callback returns this littlefs error */
extern int UT_Storage_FailReads;
extern int UT_Storage_FailProgs;

#endif /* UT_ADAPTOR_STORAGE_H */
