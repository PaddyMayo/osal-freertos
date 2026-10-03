/*
 * File: coveragetest-filesys.c
 *
 * Purpose:
 *   White-box coverage test cases for freertos/src/os-impl-filesys.c. The
 *   volume is real littlefs running over the RAM device in
 *   ../stubs/ut-storage-stubs.c, and the lock is the fake FreeRTOS mutex in
 *   ../stubs/freertos-filesys-stubs.c.
 *
 *   Device failures are scripted through ../adaptors/inc/ut-adaptor-storage.h.
 *   littlefs caches reads, so the error paths also drop the read cache first,
 *   which makes the next read reach the device.
 */

#include <string.h>

#include "utassert.h"
#include "uttest.h"
#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "lfs.h"

#include "osapi-idmap.h"
#include "os-shared-filesys.h"
#include "os-shared-idmap.h"
#include "os-impl-filesys.h"

#include "ut-adaptor-storage.h"

/* Geometry of the RAM device in ut-storage-stubs.c */
#define UT_FS_BLOCK_SIZE  512
#define UT_FS_BLOCK_COUNT 64

#define UT_FS_TOKEN(idx)                                                                                   \
    (OS_object_token_t)                                                                                    \
    {                                                                                                      \
        .obj_type = OS_OBJECT_TYPE_OS_FILESYS, .obj_id = (osal_id_t) { 0x30000 + (idx) }, .obj_idx = (idx) \
    }

#define UT_INDEX_0 OSAL_INDEX_C(0)

/* Makes the next littlefs read go to the device, not the read cache */
static void UT_FsTest_DropReadCache(void)
{
    OS_impl_lfs.rcache.block = (lfs_block_t)-1;
}

/*
**********************************************************************************
**          TEST CASES
**********************************************************************************
*/

void Test_OS_FileSysStartVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    UT_Storage_Reset();

    /* before the first start there is no lock, so the volume refuses everything */
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_ERROR);

    /* first start creates the mutex and publishes the device geometry */
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutexStatic, 1);
    UtAssert_UINT32_EQ(OS_filesys_table[UT_INDEX_0].blocksize, UT_FS_BLOCK_SIZE);
    UtAssert_UINT32_EQ(OS_filesys_table[UT_INDEX_0].numblocks, UT_FS_BLOCK_COUNT);

    /* a second start reuses the existing mutex */
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_SUCCESS);
    UtAssert_STUB_COUNT(xSemaphoreCreateMutexStatic, 1);

    /* a cache bigger than each open file reserves is refused */
    UT_Storage_SetCacheSize(OS_FREERTOS_LFS_FILE_CACHE_SIZE + 1);
    UtAssert_INT32_EQ(OS_FileSysStartVolume_Impl(&token), OS_ERROR);
    UT_Storage_Reset();
}

void Test_OS_FileSysMountVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    /* zeroed device: littlefs finds it corrupt, so it is formatted and mounted */
    UT_Storage_Reset();
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_SUCCESS);
    OS_FreeRTOS_LfsRelease();

    /* already mounted -> success without touching the device */
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);

    /* a read failure is not corruption: the device is left alone and the mount fails */
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);
    UT_Storage_FailReads = LFS_ERR_IO;
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_ERROR);
    UT_Storage_FailReads = 0;

    /* corrupt device whose format also fails -> mount fails */
    UT_Storage_Reset();
    UT_Storage_FailProgs = LFS_ERR_IO;
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_ERROR);
    UT_Storage_Reset();

    /* leave the volume mounted for the next test */
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
}

void Test_OS_FileSysUnmountVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    UT_Storage_Reset();

    /* not mounted: nothing to do */
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);

    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);

    /* once unmounted the lock refuses file access */
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_ERROR);
}

void Test_OS_FileSysStopVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    UT_Storage_Reset();
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);

    /* stopping a mounted volume unmounts it */
    UtAssert_INT32_EQ(OS_FileSysStopVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_ERROR);

    /* stopping again is harmless */
    UtAssert_INT32_EQ(OS_FileSysStopVolume_Impl(&token), OS_SUCCESS);
}

void Test_OS_FileSysFormatVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    /* not mounted: format straight away */
    UT_Storage_Reset();
    UtAssert_INT32_EQ(OS_FileSysFormatVolume_Impl(&token), OS_SUCCESS);

    /* mounted: format unmounts first, and leaves the volume unmounted */
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysFormatVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_ERROR);

    /* a failing device fails the format */
    UT_Storage_FailProgs = LFS_ERR_IO;
    UtAssert_INT32_EQ(OS_FileSysFormatVolume_Impl(&token), OS_ERROR);
    UT_Storage_Reset();

    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
}

void Test_OS_FileSysCheckVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    /* not mounted: nothing to check */
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, false), OS_ERROR);

    /* mounted and consistent; repair is not supported and is ignored */
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, true), OS_SUCCESS);

    /* the walk reads the device, and a failing read is reported */
    UT_FsTest_DropReadCache();
    UT_Storage_FailReads = LFS_ERR_IO;
    UtAssert_INT32_EQ(OS_FileSysCheckVolume_Impl(&token, false), OS_ERROR);
    UT_Storage_FailReads = 0;
}

void Test_OS_FileSysStatVolume_Impl(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);
    OS_statvfs_t      stat;

    /* not mounted: nothing to report */
    UtAssert_INT32_EQ(OS_FileSysUnmountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &stat), OS_ERROR);

    /* mounted: geometry comes from the device, and some blocks are free */
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &stat), OS_SUCCESS);
    UtAssert_UINT32_EQ(stat.block_size, UT_FS_BLOCK_SIZE);
    UtAssert_True(stat.total_blocks == UT_FS_BLOCK_COUNT, "total blocks come from the device");
    UtAssert_True(stat.blocks_free > 0 && stat.blocks_free < UT_FS_BLOCK_COUNT, "some blocks are in use, some free");

    /* the size walk reads the device, and a failing read is reported */
    UT_FsTest_DropReadCache();
    UT_Storage_FailReads = LFS_ERR_IO;
    UtAssert_INT32_EQ(OS_FileSysStatVolume_Impl(&token, &stat), OS_ERROR);
    UT_Storage_FailReads = 0;
}

void Test_OS_FreeRTOS_LfsAcquire(void)
{
    OS_object_token_t token = UT_FS_TOKEN(0);

    UT_Storage_Reset();
    UtAssert_INT32_EQ(OS_FileSysMountVolume_Impl(&token), OS_SUCCESS);

    /* mounted: the lock is taken, and Release() gives it back */
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsAcquire(), OS_SUCCESS);
    OS_FreeRTOS_LfsRelease();
}

void Test_OS_FreeRTOS_LfsResult(void)
{
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsResult(LFS_ERR_OK), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsResult(7), OS_SUCCESS);
    UtAssert_INT32_EQ(OS_FreeRTOS_LfsResult(LFS_ERR_NOENT), OS_ERROR);
}

/* ------------------- End of test cases --------------------------------------*/

/*
 * Each case starts from an unmounted volume over a zeroed device, so no state
 * leaks between cases. The lock itself is created once and kept, since the
 * first-start case in this module depends on it not existing yet.
 */
void Osapi_Test_Setup(void)
{
    OS_object_token_t fs = UT_FS_TOKEN(0);

    UT_ResetState(0);
    (void)OS_FileSysStopVolume_Impl(&fs);
    UT_Storage_Reset();
}

void Osapi_Test_Teardown(void)
{
}

#define ADD_TEST(test) UtTest_Add((Test_##test), Osapi_Test_Setup, Osapi_Test_Teardown, #test)

/*
**********************************************************************************
**          TEST SETUP
**********************************************************************************
*/

void UtTest_Setup(void)
{
    /* The lock is created by the first start, so Acquire-before-start is tested first */
    ADD_TEST(OS_FileSysStartVolume_Impl);
    ADD_TEST(OS_FileSysMountVolume_Impl);
    ADD_TEST(OS_FileSysUnmountVolume_Impl);
    ADD_TEST(OS_FileSysStopVolume_Impl);
    ADD_TEST(OS_FileSysFormatVolume_Impl);
    ADD_TEST(OS_FileSysCheckVolume_Impl);
    ADD_TEST(OS_FileSysStatVolume_Impl);
    ADD_TEST(OS_FreeRTOS_LfsAcquire);
    ADD_TEST(OS_FreeRTOS_LfsResult);
}
