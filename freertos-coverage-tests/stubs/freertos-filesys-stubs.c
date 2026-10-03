/*
 * File: freertos-filesys-stubs.c
 *
 * Purpose:
 *   UT-controllable stand-ins for the FreeRTOS mutex calls os-impl-filesys.c
 *   makes around every littlefs call. Follows freertos-common-stubs.c: a
 *   scripted failure through UT_SetDeferredRetcode() where it matters, and
 *   success otherwise.
 */

#include "utstubs.h"

#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t OCS_xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
    int32 status;

    status = UT_DEFAULT_IMPL(xSemaphoreCreateMutexStatic);

    if (status < 0)
    {
        return NULL;
    }

    return (SemaphoreHandle_t)pxMutexBuffer;
}

BaseType_t OCS_xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    (void)xSemaphore;
    (void)xBlockTime;

    UT_DEFAULT_IMPL(xSemaphoreTake);

    return pdTRUE;
}

BaseType_t OCS_xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    (void)xSemaphore;

    UT_DEFAULT_IMPL(xSemaphoreGive);

    return pdTRUE;
}
